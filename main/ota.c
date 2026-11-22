/*
#####################################################################################
#
# Copyright (c) 2023-2026 Liu Chuansen <liuchuansen@outlook.com>. All Rights Reserved.
#
# This software is the personal work of Liu Chuansen. You shall not copy, use,
# distribute, or create derivative works of this software without the author's
# prior written permission.
#
# UNAUTHORIZED COPYING, USE, OR DISTRIBUTION OF THIS SOFTWARE, OR ANY PORTION OR
# DERIVATIVE THEREOF, IS STRICTLY PROHIBITED. IF YOU HAVE RECEIVED THIS SOFTWARE IN
# ERROR, PLEASE NOTIFY THE AUTHOR IMMEDIATELY AND DELETE IT FROM YOUR SYSTEM.
#
#####################################################################################
#
# Description: AgentMate UART OTA writing, image validation, and startup rollback
# Author: Liu Chuansen <liuchuansen@outlook.com>
#
#####################################################################################
#
# Revision History:
#
# Date       Version  Author       Description
# ---------- -------- ------------ -----------------------------------------------
# 2026-10-12 1.0      Codex         Created UART OTA implementation.
#
#####################################################################################
*/

#include "ota.h"
#include "xmodem.h"

#include <stdio.h>
#include <string.h>

#include "driver/uart.h"
#include "esp_app_format.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/task.h"
#include "mbedtls/sha256.h"

static struct {
    const esp_partition_t *partition;
    esp_ota_handle_t handle;
    bool handle_active;
    mbedtls_sha256_context sha;
    bool sha_active;
    uint8_t digest[32];
    struct am_ota_view view;
    struct am_xmodem receiver;
    bool serial;
    bool draining;
    bool skip_lf;
    int64_t quiet_us;
    int64_t drain_started_us;
    bool input_flushed;
} s_ota;

const char *am_ota_state_name(enum am_ota_state state)
{
    static const char *const s_names[] = {"IDLE", "RECEIVING", "VERIFYING", "READY", "FAILED"};
    return state >= AM_OTA_IDLE && state <= AM_OTA_FAILED ? s_names[state] : "FAILED";
}

uint32_t am_ota_max_size(void)
{
    const esp_partition_t *partition = esp_ota_get_next_update_partition(NULL);
    return partition ? partition->size : 0;
}

static void publish(am_app_t *app)
{
    xSemaphoreTake(app->model_lock, portMAX_DELAY);
    app->model.ota = s_ota.view;
    app->model.revision++;
    xSemaphoreGive(app->model_lock);
}

static void release(void)
{
    if (s_ota.handle_active) {
        (void)esp_ota_abort(s_ota.handle);
        s_ota.handle_active = false;
    }
    if (s_ota.sha_active) {
        mbedtls_sha256_free(&s_ota.sha);
        s_ota.sha_active = false;
    }
}

static void failed(am_app_t *app, const char *error)
{
    release();
    s_ota.view.state = AM_OTA_FAILED;
    s_ota.view.changed_us = esp_timer_get_time();
    snprintf(s_ota.view.error, sizeof(s_ota.view.error), "%s", error);
    publish(app);
}

static bool valid_version(const char *version)
{
    if (!version || version[0] != 'v' || strlen(version) >= sizeof(s_ota.view.version)) {
        return false;
    }
    bool dot = false;
    unsigned digits = 0;
    for (const char *p = version + 1; *p; ++p) {
        if (*p == '.' && !dot && digits) {
            dot = true;
            digits = 0;
        } else if (*p >= '0' && *p <= '9') {
            digits++;
        } else {
            return false;
        }
    }
    return dot && digits;
}

static int hex_digit(char byte)
{
    if (byte >= '0' && byte <= '9') {
        return byte - '0';
    }
    if (byte >= 'a' && byte <= 'f') {
        return byte - 'a' + 10;
    }
    return -1;
}

const char *am_ota_begin(am_app_t *app, const char *version, uint32_t size, const char *sha256)
{
    if (s_ota.serial || s_ota.view.state == AM_OTA_READY || s_ota.view.state == AM_OTA_RECEIVING) {
        return "BAD_STATE";
    }
    uint8_t digest[32];
    if (!valid_version(version) || !sha256 || strlen(sha256) != 64 || size < sizeof(esp_image_header_t) +
        sizeof(esp_image_segment_header_t) + sizeof(esp_app_desc_t)) {
        return "INVALID_ARG";
    }
    for (unsigned i = 0; i < sizeof(digest); ++i) {
        int high = hex_digit(sha256[i * 2]), low = hex_digit(sha256[i * 2 + 1]);
        if (high < 0 || low < 0) {
            return "INVALID_ARG";
        }
        digest[i] = (uint8_t)((high << 4) | low);
    }
    s_ota.partition = esp_ota_get_next_update_partition(NULL);
    if (!s_ota.partition || size > s_ota.partition->size) {
        return "OTA_SIZE";
    }
    release();
    memset(&s_ota.view, 0, sizeof(s_ota.view));
    s_ota.view.size = size;
    snprintf(s_ota.view.version, sizeof(s_ota.view.version), "%s", version);
    memcpy(s_ota.digest, digest, sizeof(digest));
    esp_err_t err = esp_ota_begin(s_ota.partition, size, &s_ota.handle);
    if (err != ESP_OK) {
        failed(app, err == ESP_ERR_OTA_ROLLBACK_INVALID_STATE ? "OTA_PENDING_VERIFY" : "OTA_PREPARE");
        return s_ota.view.error;
    }
    s_ota.handle_active = true;
    mbedtls_sha256_init(&s_ota.sha);
    s_ota.sha_active = true;
    if (mbedtls_sha256_starts(&s_ota.sha, 0) != 0) {
        failed(app, "OTA_HASH");
        return s_ota.view.error;
    }
    s_ota.view.state = AM_OTA_RECEIVING;
    s_ota.view.changed_us = esp_timer_get_time();
    publish(app);
    return NULL;
}

static void send_byte(void *context, uint8_t byte)
{
    (void)context;
    (void)uart_write_bytes(UART_NUM_0, &byte, 1);
}

static bool write_data(void *context, const uint8_t *data, size_t length)
{
    am_app_t *app = context;
    if (esp_ota_write(s_ota.handle, data, length) != ESP_OK || mbedtls_sha256_update(&s_ota.sha, data, length) != 0) {
        return false;
    }
    s_ota.view.received += length;
    publish(app);
    return true;
}

bool am_ota_serial_active(void)
{
    return s_ota.serial;
}

void am_ota_start_serial(am_app_t *app, bool skip_lf)
{
    struct am_xmodem_io io = {.context = app, .send = send_byte, .write = write_data};
    s_ota.serial = true;
    s_ota.draining = false;
    s_ota.input_flushed = false;
    s_ota.skip_lf = skip_lf;
    am_xmodem_start(&s_ota.receiver, &io, s_ota.view.size, esp_timer_get_time());
}

void am_ota_serial_feed(am_app_t *app, const uint8_t *data, size_t length)
{
    (void)app;
    for (size_t i = 0; i < length; ++i) {
        int64_t now = esp_timer_get_time();
        if (s_ota.skip_lf) {
            s_ota.skip_lf = false;
            if (data[i] == '\n') {
                continue;
            }
        }
        if (!s_ota.draining) {
            am_xmodem_feed(&s_ota.receiver, data[i], now);
        }
        s_ota.quiet_us = now;
    }
}

static void verify_image(am_app_t *app)
{
    s_ota.view.state = AM_OTA_VERIFYING;
    s_ota.view.changed_us = esp_timer_get_time();
    publish(app);
    uint8_t digest[32];
    if (mbedtls_sha256_finish(&s_ota.sha, digest) != 0 || memcmp(digest, s_ota.digest, sizeof(digest)) != 0) {
        failed(app, "OTA_HASH");
        return;
    }
    esp_err_t err = esp_ota_end(s_ota.handle);
    /* esp_ota_end frees the handle on both success and failure. */
    s_ota.handle_active = false;
    release();
    if (err != ESP_OK) {
        failed(app, "OTA_IMAGE");
        return;
    }
    esp_image_header_t header;
    esp_app_desc_t description;
    const esp_app_desc_t *running = esp_app_get_description();
    if (esp_partition_read(s_ota.partition, 0, &header, sizeof(header)) != ESP_OK || header.chip_id != ESP_CHIP_ID_ESP32C3 ||
        esp_ota_get_partition_description(s_ota.partition, &description) != ESP_OK ||
        memcmp(description.project_name, running->project_name, sizeof(description.project_name)) != 0 ||
        strncmp(description.version, s_ota.view.version + 1, sizeof(description.version)) != 0) {
        failed(app, "OTA_IDENTITY");
        return;
    }
    s_ota.view.state = AM_OTA_READY;
    s_ota.view.changed_us = esp_timer_get_time();
    publish(app);
}

bool am_ota_serial_tick(am_app_t *app)
{
    int64_t now = esp_timer_get_time();
    if (!s_ota.draining) {
        am_xmodem_tick(&s_ota.receiver, now);
        if (s_ota.receiver.result == AM_XMODEM_ACTIVE) {
            return false;
        }
        if (s_ota.receiver.result == AM_XMODEM_COMPLETE) {
            verify_image(app);
        } else {
            failed(app, s_ota.receiver.error);
        }
        s_ota.draining = true;
        s_ota.quiet_us = esp_timer_get_time();
        s_ota.drain_started_us = s_ota.quiet_us;
    }
    if (!s_ota.input_flushed && esp_timer_get_time() - s_ota.drain_started_us >= 2000000) {
        (void)uart_flush_input(UART_NUM_0);
        s_ota.input_flushed = true;
        s_ota.quiet_us = esp_timer_get_time();
    }
    /* Polling every 100 ms drains queued bytes. Never parse until the line is quiet. */
    if (esp_timer_get_time() - s_ota.quiet_us < 200000) {
        return false;
    }
    s_ota.serial = false;
    s_ota.draining = false;
    xSemaphoreTake(app->model_lock, portMAX_DELAY);
    app->last_valid_command_us = esp_timer_get_time();
    xSemaphoreGive(app->model_lock);
    return true;
}

void am_ota_abort(am_app_t *app)
{
    release();
    memset(&s_ota.view, 0, sizeof(s_ota.view));
    s_ota.partition = NULL;
    publish(app);
}

const char *am_ota_commit(am_app_t *app)
{
    if (s_ota.view.state != AM_OTA_READY || !s_ota.partition) {
        return "BAD_STATE";
    }
    if (esp_ota_set_boot_partition(s_ota.partition) != ESP_OK) {
        failed(app, "OTA_COMMIT");
        return s_ota.view.error;
    }
    app->restart_pending = true;
    return NULL;
}

static void boot_watch_task(void *argument)
{
    am_app_t *app = argument;
    int64_t started = esp_timer_get_time(), healthy_since = 0;
    for (;;) {
        int64_t now = esp_timer_get_time();
        bool healthy = false;
        if (xSemaphoreTake(app->model_lock, pdMS_TO_TICKS(100)) == pdTRUE) {
            healthy = app->model.protocol_ready && app->protocol_heartbeat_us > 0 && app->ui_heartbeat_us > 0 &&
                      now - app->protocol_heartbeat_us < 1000000 && now - app->ui_heartbeat_us < 1000000;
            xSemaphoreGive(app->model_lock);
        }
#ifdef CONFIG_AGENTMATE_OTA_TEST_FAIL_BOOT
        healthy = false;
#endif
        if (!healthy) {
            healthy_since = 0;
        } else if (!healthy_since) {
            healthy_since = now;
        } else if (now - healthy_since >= 10000000 && esp_ota_mark_app_valid_cancel_rollback() == ESP_OK) {
            vTaskDelete(NULL);
            return;
        }
        if (now - started >= 30000000) {
            (void)esp_ota_mark_app_invalid_rollback_and_reboot();
            esp_restart();
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void am_ota_boot_watch(am_app_t *app)
{
    esp_ota_img_states_t state;
    if (esp_ota_get_state_partition(esp_ota_get_running_partition(), &state) == ESP_OK && state == ESP_OTA_IMG_PENDING_VERIFY) {
        configASSERT(xTaskCreate(boot_watch_task, "ota_boot_check", 3072, app, 6, NULL) == pdPASS);
    }
}
