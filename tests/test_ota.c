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
# Description: AgentMate OTA lifecycle and startup health tests with mocked ESP-IDF
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

#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
#include <string.h>
#include "../main/ota.c"

static am_app_t s_app;
static int64_t s_now = 1;
static esp_partition_t s_partition = {.size = 0x1a0000};
static esp_app_desc_t s_description = {.version = "2.1", .project_name = "agentmate_esp32"};
static uint8_t s_image[512], s_flash[512], s_wire[4096];
static size_t s_length, s_wire_length;
static unsigned s_abort_count, s_end_count, s_commit_count, s_free_count;
static esp_err_t s_prepare_error, s_write_error, s_end_error, s_commit_error;
static bool s_handle_active, s_bad_identity, s_bad_chip;
static bool s_pending, s_keep_healthy, s_confirmed, s_deleted, s_rolled_back, s_created;
static jmp_buf s_restart;

int64_t esp_timer_get_time(void) { return s_now; }
const esp_partition_t *esp_ota_get_next_update_partition(const esp_partition_t *partition)
{
    (void)partition;
    return &s_partition;
}
const esp_partition_t *esp_ota_get_running_partition(void) { return &s_partition; }
const esp_app_desc_t *esp_app_get_description(void) { return &s_description; }
esp_err_t esp_ota_begin(const esp_partition_t *partition, size_t length, esp_ota_handle_t *handle)
{
    assert(partition == &s_partition && length <= s_partition.size && !s_handle_active);
    if (s_prepare_error) {
        return s_prepare_error;
    }
    *handle = 7;
    s_handle_active = true;
    s_length = 0;
    return ESP_OK;
}
esp_err_t esp_ota_abort(esp_ota_handle_t handle)
{
    assert(handle == 7 && s_handle_active);
    s_handle_active = false;
    s_abort_count++;
    return ESP_OK;
}
esp_err_t esp_ota_write(esp_ota_handle_t handle, const void *bytes, size_t length)
{
    assert(handle == 7 && s_handle_active && s_length + length <= sizeof(s_flash));
    if (s_write_error) {
        return s_write_error;
    }
    memcpy(s_flash + s_length, bytes, length);
    s_length += length;
    return ESP_OK;
}
esp_err_t esp_ota_end(esp_ota_handle_t handle)
{
    assert(handle == 7 && s_handle_active);
    s_handle_active = false;
    s_end_count++;
    return s_end_error;
}
esp_err_t esp_partition_read(const esp_partition_t *partition, size_t offset, void *bytes, size_t length)
{
    assert(partition == &s_partition && offset == 0 && length == sizeof(esp_image_header_t));
    esp_image_header_t header = {.chip_id = s_bad_chip ? 0 : ESP_CHIP_ID_ESP32C3};
    memcpy(bytes, &header, length);
    return ESP_OK;
}
esp_err_t esp_ota_get_partition_description(const esp_partition_t *partition, esp_app_desc_t *description)
{
    assert(partition == &s_partition);
    *description = s_description;
    if (s_bad_identity) {
        snprintf(description->project_name, sizeof(description->project_name), "other_project");
    }
    return ESP_OK;
}
esp_err_t esp_ota_set_boot_partition(const esp_partition_t *partition)
{
    assert(partition == &s_partition && !s_handle_active);
    s_commit_count++;
    return s_commit_error;
}
esp_err_t esp_ota_get_state_partition(const esp_partition_t *partition, esp_ota_img_states_t *state)
{
    assert(partition == &s_partition);
    *state = s_pending ? ESP_OTA_IMG_PENDING_VERIFY : ESP_OTA_IMG_VALID;
    return ESP_OK;
}
esp_err_t esp_ota_mark_app_valid_cancel_rollback(void) { s_confirmed = true; return ESP_OK; }
esp_err_t esp_ota_mark_app_invalid_rollback_and_reboot(void)
{
    s_rolled_back = true;
    longjmp(s_restart, 1);
}
void esp_restart(void) { longjmp(s_restart, 1); }
esp_err_t uart_flush_input(int port) { assert(port == 0); return ESP_OK; }
int uart_write_bytes(int port, const void *bytes, size_t length)
{
    assert(port == 0 && s_wire_length + length <= sizeof(s_wire));
    memcpy(s_wire + s_wire_length, bytes, length);
    s_wire_length += length;
    return (int)length;
}
/* Deterministic mock: tests lifecycle, hash coverage and failure handling, not cryptography. */
void mbedtls_sha256_init(mbedtls_sha256_context *context) { context->sum = 0; }
void mbedtls_sha256_free(mbedtls_sha256_context *context) { (void)context; s_free_count++; }
int mbedtls_sha256_starts(mbedtls_sha256_context *context, int sha224)
{
    assert(sha224 == 0);
    context->sum = 0;
    return 0;
}
int mbedtls_sha256_update(mbedtls_sha256_context *context, const uint8_t *data, size_t length)
{
    for (size_t i = 0; i < length; ++i) {
        context->sum += data[i];
    }
    return 0;
}
int mbedtls_sha256_finish(mbedtls_sha256_context *context, uint8_t *digest)
{
    for (unsigned i = 0; i < 32; ++i) {
        digest[i] = (uint8_t)(context->sum + i);
    }
    return 0;
}
void vTaskDelay(unsigned ticks)
{
    s_now += (int64_t)ticks * 1000;
    if (s_keep_healthy) {
        s_app.ui_heartbeat_us = s_now;
        s_app.protocol_heartbeat_us = s_now;
        s_app.model.protocol_ready = true;
    }
}
void vTaskDelete(void *task) { (void)task; s_deleted = true; }
int xTaskCreate(void (*entry)(void *), const char *name, unsigned stack, void *argument, unsigned priority, void *handle)
{
    assert(entry == boot_watch_task && argument == &s_app && stack == 3072 && priority == 6 && !handle);
    (void)name;
    s_created = true;
    return pdPASS;
}

static const char *image_hash(void)
{
    static char s_hash[65];
    uint8_t sum = 0;
    for (unsigned i = 0; i < sizeof(s_image); ++i) {
        sum += s_image[i];
    }
    for (unsigned i = 0; i < 32; ++i) {
        snprintf(s_hash + i * 2, 3, "%02x", (uint8_t)(sum + i));
    }
    return s_hash;
}

static void begin(const char *version, const char *hash)
{
    s_wire_length = 0;
    assert(am_ota_begin(&s_app, version, sizeof(s_image), hash) == NULL);
    assert(s_app.model.ota.state == AM_OTA_RECEIVING);
    assert(s_ota.handle_active && s_ota.sha_active);
    am_ota_start_serial(&s_app, true);
    assert(am_ota_serial_active() && s_wire[0] == 'C');
    const uint8_t lf = '\n';
    am_ota_serial_feed(&s_app, &lf, 1);
    assert(!s_ota.receiver.resync);
}

static void send_image(void)
{
    uint8_t packet[1029] = {2, 1, 254};
    memcpy(packet + 3, s_image, sizeof(s_image));
    memset(packet + 3 + sizeof(s_image), 0x1a, 1024 - sizeof(s_image));
    uint16_t crc = am_xmodem_crc(packet + 3, 1024);
    packet[1027] = (uint8_t)(crc >> 8);
    packet[1028] = (uint8_t)crc;
    for (size_t i = 0; i < sizeof(packet); i += 17) {
        size_t count = sizeof(packet) - i;
        am_ota_serial_feed(&s_app, packet + i, count < 17 ? count : 17);
    }
    assert(s_wire[s_wire_length - 1] == 6 || s_wire[s_wire_length - 1] == 0x18);
    if (!s_write_error) {
        am_ota_serial_feed(&s_app, packet, sizeof(packet));
        assert(s_length == sizeof(s_image) && s_app.model.ota.received == sizeof(s_image));
    }
}

static void exit_serial(void)
{
    uint8_t eot = 4;
    am_ota_serial_feed(&s_app, &eot, 1);
    s_now += 2000000;
    assert(!am_ota_serial_tick(&s_app));
    const uint8_t residue[] = {'A', 'T', '+', 'M', 'O', 'D', 'E', '=', 'F', 'A', 'C', 'T', 'O', 'R', 'Y', '\r'};
    am_ota_serial_feed(&s_app, residue, sizeof(residue));
    s_now += 199999;
    assert(!am_ota_serial_tick(&s_app));
    s_now++;
    assert(am_ota_serial_tick(&s_app) && !am_ota_serial_active());
    assert(!s_handle_active && !s_ota.handle_active && !s_ota.sha_active);
}

static void failure_is(const char *error)
{
    assert(s_app.model.ota.state == AM_OTA_FAILED && strcmp(s_app.model.ota.error, error) == 0);
    assert(s_commit_count == 0 && !s_app.restart_pending);
    assert(!s_handle_active && !s_ota.sha_active);
}

int main(void)
{
    for (unsigned i = 0; i < sizeof(s_image); ++i) {
        s_image[i] = (uint8_t)(i * 3 + 1);
    }
    assert(am_ota_max_size() == 0x1a0000);
    assert(strcmp(am_ota_commit(&s_app), "BAD_STATE") == 0);
    assert(strcmp(am_ota_begin(&s_app, "2.1", 512, image_hash()), "INVALID_ARG") == 0);
    assert(strcmp(am_ota_begin(&s_app, "v2.1", 512, "AB"), "INVALID_ARG") == 0);
    assert(strcmp(am_ota_begin(&s_app, "v2.1", 0x1a0001, image_hash()), "OTA_SIZE") == 0);
    assert(!s_handle_active);
    s_prepare_error = ESP_ERR_OTA_ROLLBACK_INVALID_STATE;
    assert(strcmp(am_ota_begin(&s_app, "v2.1", 512, image_hash()), "OTA_PENDING_VERIFY") == 0);
    failure_is("OTA_PENDING_VERIFY");
    s_prepare_error = 0;

    begin("v2.1", image_hash());
    assert(strcmp(am_ota_begin(&s_app, "v2.1", 512, image_hash()), "BAD_STATE") == 0);
    send_image();
    exit_serial();
    assert(s_app.model.ota.state == AM_OTA_READY && s_commit_count == 0 && s_abort_count == 0 && s_end_count == 1);
    assert(memcmp(s_flash, s_image, sizeof(s_image)) == 0);
    assert(strcmp(am_ota_begin(&s_app, "v2.1", 512, image_hash()), "BAD_STATE") == 0);
    am_ota_abort(&s_app);
    am_ota_abort(&s_app);
    assert(s_app.model.ota.state == AM_OTA_IDLE && s_abort_count == 0);

    begin("v2.1", "0000000000000000000000000000000000000000000000000000000000000000");
    send_image();
    exit_serial();
    failure_is("OTA_HASH");
    assert(s_abort_count == 1 && s_end_count == 1);
    s_end_error = 1;
    begin("v2.1", image_hash());
    send_image();
    exit_serial();
    failure_is("OTA_IMAGE");
    assert(s_abort_count == 1);
    s_end_error = 0;
    s_bad_identity = true;
    begin("v2.1", image_hash());
    send_image();
    exit_serial();
    failure_is("OTA_IDENTITY");
    s_bad_identity = false;
    s_bad_chip = true;
    begin("v2.1", image_hash());
    send_image();
    exit_serial();
    failure_is("OTA_IDENTITY");
    s_bad_chip = false;
    begin("v2.2", image_hash());
    send_image();
    exit_serial();
    failure_is("OTA_IDENTITY");
    s_write_error = 1;
    begin("v2.1", image_hash());
    send_image();
    exit_serial();
    failure_is("OTA_WRITE");
    s_write_error = 0;
    begin("v2.1", image_hash());
    const uint8_t cancel[] = {0x18, 0x18};
    am_ota_serial_feed(&s_app, cancel, sizeof(cancel));
    exit_serial();
    failure_is("OTA_CANCELLED");
    begin("v2.1", image_hash());
    send_image();
    exit_serial();
    s_commit_error = 1;
    assert(strcmp(am_ota_commit(&s_app), "OTA_COMMIT") == 0 && !s_app.restart_pending);
    s_commit_error = 0;
    begin("v2.1", image_hash());
    send_image();
    exit_serial();
    assert(am_ota_commit(&s_app) == NULL && s_app.restart_pending && s_commit_count == 2);
    assert(s_free_count == s_end_count + s_abort_count);

    am_ota_boot_watch(&s_app);
    assert(!s_created);
    s_pending = true;
    am_ota_boot_watch(&s_app);
    assert(s_created);
    s_keep_healthy = true;
    boot_watch_task(&s_app);
    assert(s_confirmed && s_deleted && !s_rolled_back);
    s_confirmed = false;
    s_keep_healthy = false;
    s_app.model.protocol_ready = false;
    if (!setjmp(s_restart)) {
        boot_watch_task(&s_app);
        assert(false);
    }
    assert(s_rolled_back && !s_confirmed);
    puts("PASS OTA lifecycle: identity, hash, cleanup, serial quarantine, explicit commit and startup rollback");
    return 0;
}
