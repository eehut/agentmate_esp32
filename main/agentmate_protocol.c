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
# Description: AgentMate UART AT protocol, factory diagnostics, and state handling
# Author: Liu Chuansen <liuchuansen@outlook.com>
#
#####################################################################################
#
# Revision History:
#
# Date       Version  Author       Description
# ---------- -------- ------------ -----------------------------------------------
# 2026-10-01 1.0      Liu Chuansen  Organized for project C style.
# 2026-10-01 1.1      Codex         Added factory AT interaction and diagnostics.
# 2026-10-01 1.2      Codex         Report the CMake project version on HELLO and INFO.
# 2026-10-01 1.3      Codex         Use the shared product model on HELLO and INFO.
#
#####################################################################################
*/

#include "agentmate_app.h"

#include <errno.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "bootloader_random.h"
#include "driver/uart.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_rom_crc.h"
#include "esp_timer.h"
#include "esp_system.h"
#include "nvs_flash.h"

#define AM_PAIR_MAGIC 0x414d5031U
#define AM_PAIR_VERSION 1U
#define AM_CONNECTION_TIMEOUT_US (30LL * 1000000LL)

static const char *s_tag = "agentmate_at";
static bool s_last_command_failed;
static bool s_log_failed;
static char s_pending_log[160];
static uint32_t s_log_sequence;

typedef struct {
    uint32_t magic;
    uint32_t version;
    char server_id[33];
    uint8_t key[32];
    uint32_t crc;
} am_pair_blob_t;

static int64_t now_us(void)
{
    return esp_timer_get_time();
}

static void random_bytes(void *buffer, size_t length)
{
    /* No Wi-Fi/Bluetooth is running, so explicitly enable the SAR entropy source. */
    bootloader_random_enable();
    esp_fill_random(buffer, length);
    bootloader_random_disable();
}

static void random_hex(char *out, size_t bytes)
{
    uint8_t raw[16];
    static const char s_alphabet[] = "0123456789abcdef";
    random_bytes(raw, bytes);
    for (size_t i = 0; i < bytes; ++i) {
        out[i * 2] = s_alphabet[raw[i] >> 4];
        out[i * 2 + 1] = s_alphabet[raw[i] & 15];
    }
    out[bytes * 2] = 0;
}

static bool is_lower_hex(const char *value, size_t length)
{
    if (strlen(value) != length) {
        return false;
    }
    for (size_t i = 0; i < length; ++i) {
        if (!((value[i] >= '0' && value[i] <= '9') || (value[i] >= 'a' && value[i] <= 'f'))) {
            return false;
        }
    }
    return true;
}

static bool is_short_key(const char *value, size_t maximum)
{
    size_t length = strlen(value);
    if (length == 0 || length > maximum) {
        return false;
    }
    for (size_t i = 0; i < length; ++i) {
        if (!((value[i] >= '0' && value[i] <= '9') || (value[i] >= 'a' && value[i] <= 'z'))) {
            return false;
        }
    }
    return true;
}

static bool parse_u64(const char *value, uint64_t *out)
{
    if (value == NULL || *value == 0) {
        return false;
    }
    for (const char *p = value; *p; ++p) {
        if (*p < '0' || *p > '9') {
            return false;
        }
    }
    errno = 0;
    char *end = NULL;
    unsigned long long number = strtoull(value, &end, 10);
    if (errno == ERANGE || end == value || *end != 0) {
        return false;
    }
    *out = (uint64_t)number;
    return true;
}

static bool parse_bounded(const char *value, uint64_t maximum, uint64_t *out)
{
    return parse_u64(value, out) && *out <= maximum;
}

static bool write_line(const char *format, ...)
{
    char line[AM_LINE_BYTES + 1];
    va_list arguments;
    va_start(arguments, format);
    int length = vsnprintf(line, sizeof(line), format, arguments);
    va_end(arguments);
    if (length < 0 || length > AM_LINE_BYTES - 2) {
        return false;
    }
    line[length++] = '\r';
    line[length++] = '\n';
    size_t sent = 0;
    while (sent < (size_t)length) {
        int count = uart_write_bytes(UART_NUM_0, line + sent, (size_t)length - sent);
        if (count <= 0) {
            return false;
        }
        sent += (size_t)count;
    }
    return true;
}

static void send_error(const char *code)
{
    s_last_command_failed = true;
    s_log_failed = true;
    (void)write_line("+ERR:%s", code);
    (void)write_line("ERROR");
}

static void queue_log(const char *message)
{
    size_t i = 0;
    while (message[i] && i < sizeof(s_pending_log) - 1) {
        unsigned char ch = (unsigned char)message[i];
        s_pending_log[i] = ch >= 0x20 && ch <= 0x7e && ch != ',' ? ch : '_';
        ++i;
    }
    s_pending_log[i] = 0;
}

static void flush_log(am_app_t *app)
{
    if (s_pending_log[0] && app->log_enabled) {
        (void)write_line("+LOG:%" PRIu32 ",%s,agentmate,%s", ++s_log_sequence, s_log_failed ? "ERROR" : "INFO", s_pending_log);
    }
    s_pending_log[0] = 0;
    s_log_failed = false;
}

static int split_csv(char *text, char *fields[], int capacity)
{
    if (capacity <= 0) {
        return -1;
    }
    int count = 1;
    fields[0] = text;
    for (char *p = text; *p != 0; ++p) {
        if (*p == ',') {
            *p = 0;
            if (count >= capacity) {
                return -1;
            }
            fields[count++] = p + 1;
        }
    }
    return count;
}

static esp_err_t save_setting(am_app_t *app, const char *key, uint8_t value)
{
    esp_err_t err = nvs_set_u8(app->nvs, key, value);
    if (err == ESP_OK) {
        err = nvs_commit(app->nvs);
    }
    return err;
}

static void clear_remote(am_app_t *app)
{
    memset(app->model.sessions, 0, sizeof(app->model.sessions));
    memset(app->model.usages, 0, sizeof(app->model.usages));
    memset(app->model.epoch, 0, sizeof(app->model.epoch));
    memset(app->event_ids, 0, sizeof(app->event_ids));
    app->next_event_id = 0;
    app->model.session_count = 0;
    app->model.usage_count = 0;
    app->model.sync_active = false;
    app->model.sync_id = 0;
    app->model.last_sync_end_id = 0;
    app->model.alert_kind[0] = 0;
    app->model.alert_until_us = 0;
    app->model.revision++;
}

esp_err_t am_app_init(am_app_t *app)
{
    memset(app, 0, sizeof(*app));
    app->model_lock = xSemaphoreCreateMutex();
    app->display_lock = xSemaphoreCreateMutex();
    if (app->model_lock == NULL || app->display_lock == NULL) {
        return ESP_ERR_NO_MEM;
    }
    ESP_RETURN_ON_ERROR(nvs_flash_init(), s_tag, "NVS initialization failed");
    ESP_RETURN_ON_ERROR(nvs_open("agentmate", NVS_READWRITE, &app->nvs), s_tag, "NVS open failed");
    uint8_t factory = 0;
    esp_err_t mode_error = nvs_get_u8(app->nvs, "factory_mode", &factory);
    if (mode_error != ESP_OK && mode_error != ESP_ERR_NVS_NOT_FOUND) {
        return mode_error;
    }
    if (mode_error == ESP_OK && factory > 1) {
        return ESP_ERR_INVALID_STATE;
    }
    bool mode_saved = mode_error == ESP_OK;
    size_t id_length = sizeof(app->device_id);
    esp_err_t err = nvs_get_str(app->nvs, "device_id", app->device_id, &id_length);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        app->model.factory_mode = !mode_saved || factory != 0;
        random_hex(app->device_id, 16);
        ESP_RETURN_ON_ERROR(nvs_set_str(app->nvs, "device_id", app->device_id), s_tag, "device ID save failed");
        ESP_RETURN_ON_ERROR(nvs_commit(app->nvs), s_tag, "device ID commit failed");
    } else if (err != ESP_OK || !is_lower_hex(app->device_id, 32)) {
        return ESP_ERR_INVALID_STATE;
    } else {
        app->model.factory_mode = mode_saved && factory != 0;
    }
    am_pair_blob_t blob;
    size_t pair_length = sizeof(blob);
    err = nvs_get_blob(app->nvs, "pair", &blob, &pair_length);
    if (err == ESP_OK && pair_length == sizeof(blob) && blob.magic == AM_PAIR_MAGIC && blob.version == AM_PAIR_VERSION && blob.server_id[32] == 0 &&
        is_lower_hex(blob.server_id, 32) && blob.crc == esp_rom_crc32_le(0, (const uint8_t *)&blob, offsetof(am_pair_blob_t, crc))) {
        memcpy(app->owner_server_id, blob.server_id, 33);
        memcpy(app->device_key, blob.key, 32);
        app->paired = true;
    } else if (err != ESP_ERR_NVS_NOT_FOUND && err != ESP_OK) {
        return err;
    }
    app->tft_brightness = 192;
    app->led_brightness = 64;
    uint8_t value;
    if (nvs_get_u8(app->nvs, "tft_bright", &value) == ESP_OK) {
        app->tft_brightness = value;
    }
    if (nvs_get_u8(app->nvs, "led_bright", &value) == ESP_OK) {
        app->led_brightness = value;
    }
    if (nvs_get_u8(app->nvs, "rotation", &value) == ESP_OK && value <= 3) {
        app->rotation = (uint16_t)value * 90U;
    }
    app->model.link = AM_LINK_WAIT_HOST;
    app->model.test = AM_TEST_PAGE;
    app->echo_enabled = app->model.factory_mode;
    app->log_enabled = app->model.factory_mode;
    return ESP_OK;
}

static am_session_t *find_session(am_app_t *app, const char *key)
{
    for (int i = 0; i < AM_MAX_SESSIONS; ++i) {
        if (app->model.sessions[i].used && strcmp(app->model.sessions[i].key, key) == 0) {
            return &app->model.sessions[i];
        }
    }
    return NULL;
}

static am_usage_t *find_usage(am_app_t *app, const char *provider, const char *metric, const char *window)
{
    for (int i = 0; i < AM_MAX_USAGE_ENTRIES; ++i) {
        am_usage_t *entry = &app->model.usages[i];
        if (entry->used && strcmp(entry->provider, provider) == 0 && strcmp(entry->metric, metric) == 0 && strcmp(entry->window, window) == 0) {
            return entry;
        }
    }
    return NULL;
}

static bool valid_session_state(const char *value)
{
    static const char *const s_allowed[] = {
        "initializing", "ready", "working", "waiting_for_input", "permission_required", "stale", "ended", "offline",
    };
    for (size_t i = 0; i < sizeof(s_allowed) / sizeof(s_allowed[0]); ++i) {
        if (strcmp(value, s_allowed[i]) == 0) {
            return true;
        }
    }
    return false;
}

static bool valid_task(const char *value)
{
    return strcmp(value, "-") == 0 || strcmp(value, "completed") == 0 || strcmp(value, "failed") == 0 || strcmp(value, "cancelled") == 0;
}

static bool valid_provider(const char *value)
{
    return strcmp(value, "codex") == 0 || strcmp(value, "claude") == 0;
}

static bool valid_event_kind(const char *value)
{
    return strcmp(value, "permission_required") == 0 || strcmp(value, "completed") == 0 || strcmp(value, "failed") == 0 || strcmp(value, "cancelled") == 0;
}

static bool same_epoch(am_app_t *app, const char *epoch)
{
    return is_lower_hex(epoch, 32) && strcmp(app->model.epoch, epoch) == 0;
}

static void count_entries(am_app_t *app)
{
    int sessions = 0, usages = 0;
    for (int i = 0; i < AM_MAX_SESSIONS; ++i) {
        sessions += app->model.sessions[i].used;
    }
    for (int i = 0; i < AM_MAX_USAGE_ENTRIES; ++i) {
        usages += app->model.usages[i].used;
    }
    app->model.session_count = sessions;
    app->model.usage_count = usages;
}

static bool valid_nonce(const char *value)
{
    size_t length = strlen(value);
    return length > 0 && length <= 32 && is_lower_hex(value, length);
}

static void handle_hello(am_app_t *app, char *argument)
{
    char *f[2];
    if (split_csv(argument, f, 2) != 2 || strcmp(f[0], AM_PROTOCOL_VERSION_TEXT) != 0 || !is_lower_hex(f[1], 32)) {
        send_error(strcmp(f[0], AM_PROTOCOL_VERSION_TEXT) == 0 ? "INVALID_ARG" : "UNSUPPORTED_VERSION");
        return;
    }
    if (!app->model.factory_mode) {
        clear_remote(app);
        app->model.link = AM_LINK_HANDSHAKE;
    }
    (void)write_line("+HELLO:%s,agentmate,%s,%s,%s,%s,%d,%d,%s", AM_PROTOCOL_VERSION_TEXT, f[1], app->device_id, AM_PRODUCT_MODEL,
                     AM_SOFTWARE_VERSION, AM_MAX_SESSIONS, AM_MAX_USAGE_ENTRIES, app->model.factory_mode ? "FACTORY" : "NORMAL");
    (void)write_line("OK");
    queue_log("hello accepted");
}

static void handle_sync_begin(am_app_t *app, char *argument)
{
    char *f[2];
    uint64_t id;
    if (split_csv(argument, f, 2) != 2 || !is_lower_hex(f[0], 32) || !parse_u64(f[1], &id) || id == 0) {
        send_error("INVALID_ARG");
        return;
    }
    if (app->model.sync_active) {
        send_error("BAD_STATE");
        return;
    }
    if (app->model.epoch[0] && strcmp(app->model.epoch, f[0]) == 0 && id <= app->model.last_sync_end_id) {
        send_error("BAD_STATE");
        return;
    }
    if (strcmp(app->model.epoch, f[0]) != 0) {
        clear_remote(app);
        memcpy(app->model.epoch, f[0], 33);
    }
    for (int i = 0; i < AM_MAX_SESSIONS; ++i) {
        app->model.sessions[i].seen = false;
    }
    app->model.sync_active = true;
    app->model.sync_id = id;
    app->model.link = AM_LINK_SYNCING;
    app->model.revision++;
    (void)write_line("OK");
}

static void handle_session(am_app_t *app, char *argument)
{
    char *f[12];
    uint64_t rev, finished, duration, ctx_used = 0, ctx_window = 0, ctx_pct = 0, ctx_updated = 0;
    if (split_csv(argument, f, 12) != 12 || !is_lower_hex(f[0], 32) || !is_short_key(f[1], 8) || !parse_u64(f[2], &rev) || rev == 0 || !valid_provider(f[3]) ||
        !valid_session_state(f[4]) || !valid_task(f[5]) || !parse_u64(f[6], &finished) || !parse_bounded(f[7], UINT32_MAX, &duration) ||
        ((strcmp(f[5], "-") == 0) != (finished == 0))) {
        send_error("INVALID_ARG");
        return;
    }
    uint8_t mode;
    if (strcmp(f[3], "codex") == 0) {
        if (f[8][0] || f[9][0] || f[10][0] || f[11][0]) {
            send_error("INVALID_ARG");
            return;
        }
        mode = 0;
    } else if (strcmp(f[8], "?") == 0 && strcmp(f[9], "?") == 0 && strcmp(f[10], "?") == 0 && strcmp(f[11], "?") == 0) {
        mode = 1;
    } else if (parse_u64(f[8], &ctx_used) && parse_u64(f[9], &ctx_window) && ctx_window > 0 && parse_bounded(f[10], 10000, &ctx_pct) &&
               parse_u64(f[11], &ctx_updated)) {
        mode = 2;
    } else {
        send_error("INVALID_ARG");
        return;
    }
    if (!same_epoch(app, f[0])) {
        send_error("BAD_STATE");
        return;
    }
    am_session_t *entry = find_session(app, f[1]);
    if (entry && rev < entry->rev) {
        send_error("STALE_REV");
        return;
    }
    if (entry && rev == entry->rev) {
        if (app->model.sync_active) {
            entry->seen = true;
        }
        (void)write_line("OK");
        return;
    }
    if (!entry) {
        for (int i = 0; i < AM_MAX_SESSIONS; ++i) {
            if (!app->model.sessions[i].used) {
                entry = &app->model.sessions[i];
                break;
            }
        }
        if (!entry) {
            send_error("CAPACITY");
            return;
        }
    }
    memset(entry, 0, sizeof(*entry));
    entry->used = true;
    entry->seen = app->model.sync_active;
    memcpy(entry->key, f[1], strlen(f[1]) + 1);
    entry->rev = rev;
    memcpy(entry->provider, f[3], strlen(f[3]) + 1);
    memcpy(entry->state, f[4], strlen(f[4]) + 1);
    memcpy(entry->last_task, f[5], strlen(f[5]) + 1);
    entry->finished_at = finished;
    entry->task_seconds = (uint32_t)duration;
    entry->task_base_us = now_us();
    entry->context_mode = mode;
    entry->ctx_used = ctx_used;
    entry->ctx_window = ctx_window;
    entry->ctx_pct_bp = (uint16_t)ctx_pct;
    entry->ctx_updated_at = ctx_updated;
    count_entries(app);
    app->model.revision++;
    (void)write_line("OK");
}

static void handle_delete(am_app_t *app, char *argument)
{
    char *f[2];
    if (split_csv(argument, f, 2) != 2 || !is_lower_hex(f[0], 32) || !is_short_key(f[1], 8)) {
        send_error("INVALID_ARG");
        return;
    }
    if (!same_epoch(app, f[0])) {
        send_error("BAD_STATE");
        return;
    }
    am_session_t *entry = find_session(app, f[1]);
    if (entry) {
        memset(entry, 0, sizeof(*entry));
        count_entries(app);
        app->model.revision++;
    }
    (void)write_line("OK");
}

static void handle_sync_end(am_app_t *app, char *argument)
{
    char *f[2];
    uint64_t id;
    if (split_csv(argument, f, 2) != 2 || !is_lower_hex(f[0], 32) || !parse_u64(f[1], &id) || id == 0) {
        send_error("INVALID_ARG");
        return;
    }
    if (!same_epoch(app, f[0])) {
        send_error("BAD_STATE");
        return;
    }
    if (!app->model.sync_active && id == app->model.last_sync_end_id) {
        (void)write_line("OK");
        return;
    }
    if (!app->model.sync_active || id != app->model.sync_id) {
        send_error("BAD_STATE");
        return;
    }
    for (int i = 0; i < AM_MAX_SESSIONS; ++i) {
        am_session_t *entry = &app->model.sessions[i];
        if (entry->used && !entry->seen) {
            memset(entry, 0, sizeof(*entry));
        }
    }
    count_entries(app);
    app->model.sync_active = false;
    app->model.last_sync_end_id = id;
    app->model.link = app->model.clock_valid ? AM_LINK_ONLINE : AM_LINK_DEGRADED;
    app->model.revision++;
    (void)write_line("OK");
}

static void handle_list(am_app_t *app)
{
    (void)write_line("+LIST:%s,%d,%d", app->model.epoch[0] ? app->model.epoch : "-", app->model.session_count, AM_MAX_SESSIONS);
    for (int i = 0; i < AM_MAX_SESSIONS; ++i) {
        am_session_t *entry = &app->model.sessions[i];
        if (entry->used) {
            (void)write_line("+ITEM:%s,%llu", entry->key, (unsigned long long)entry->rev);
        }
    }
    (void)write_line("OK");
}

static void handle_get(am_app_t *app, char *key)
{
    if (!is_short_key(key, 8)) {
        send_error("INVALID_ARG");
        return;
    }
    am_session_t *s = find_session(app, key);
    if (!s) {
        send_error("NOT_FOUND");
        return;
    }
    char context[80];
    if (s->context_mode == 0) {
        snprintf(context, sizeof(context), ",,,");
    } else if (s->context_mode == 1) {
        snprintf(context, sizeof(context), "?,?,?,?");
    } else {
        snprintf(context, sizeof(context), "%llu,%llu,%u,%llu", (unsigned long long)s->ctx_used, (unsigned long long)s->ctx_window, s->ctx_pct_bp,
                 (unsigned long long)s->ctx_updated_at);
    }
    (void)write_line("+SESSION:%s,%s,%llu,%s,%s,%s,%llu,%u,%s", app->model.epoch, s->key, (unsigned long long)s->rev, s->provider, s->state, s->last_task,
                     (unsigned long long)s->finished_at, s->task_seconds, context);
    (void)write_line("OK");
}

static void handle_usage(am_app_t *app, char *argument)
{
    char *f[8];
    uint64_t used = 0, reset, window, updated;
    bool unknown;
    if (split_csv(argument, f, 8) != 8 || !is_lower_hex(f[0], 32) || !is_short_key(f[1], 16) || !is_short_key(f[2], 8) || !is_short_key(f[3], 8) ||
        !parse_u64(f[5], &reset) || !parse_bounded(f[6], UINT32_MAX, &window) || !parse_u64(f[7], &updated)) {
        send_error("INVALID_ARG");
        return;
    }
    unknown = strcmp(f[4], "-") == 0;
    if ((!unknown && !parse_bounded(f[4], 10000, &used)) || (unknown && (reset != 0 || window != 0))) {
        send_error("INVALID_ARG");
        return;
    }
    if (!same_epoch(app, f[0])) {
        send_error("BAD_STATE");
        return;
    }
    am_usage_t *entry = find_usage(app, f[1], f[2], f[3]);
    if (!entry) {
        for (int i = 0; i < AM_MAX_USAGE_ENTRIES; ++i) {
            if (!app->model.usages[i].used) {
                entry = &app->model.usages[i];
                break;
            }
        }
        if (!entry) {
            send_error("CAPACITY");
            return;
        }
    }
    memset(entry, 0, sizeof(*entry));
    entry->used = true;
    memcpy(entry->provider, f[1], strlen(f[1]) + 1);
    memcpy(entry->metric, f[2], strlen(f[2]) + 1);
    memcpy(entry->window, f[3], strlen(f[3]) + 1);
    entry->used_bp = unknown ? -1 : (int32_t)used;
    entry->reset_at = reset;
    entry->window_seconds = (uint32_t)window;
    entry->updated_at = updated;
    count_entries(app);
    app->model.revision++;
    (void)write_line("OK");
}

static void handle_usage_clear(am_app_t *app, char *argument)
{
    char *f[2];
    if (split_csv(argument, f, 2) != 2 || !is_lower_hex(f[0], 32) || !is_short_key(f[1], 16)) {
        send_error("INVALID_ARG");
        return;
    }
    if (!same_epoch(app, f[0])) {
        send_error("BAD_STATE");
        return;
    }
    for (int i = 0; i < AM_MAX_USAGE_ENTRIES; ++i) {
        if (app->model.usages[i].used && strcmp(app->model.usages[i].provider, f[1]) == 0) {
            memset(&app->model.usages[i], 0, sizeof(app->model.usages[i]));
        }
    }
    count_entries(app);
    app->model.revision++;
    (void)write_line("OK");
}

static void handle_event(am_app_t *app, char *argument)
{
    char *f[6];
    uint64_t occurred;
    if (split_csv(argument, f, 6) != 6 || !is_lower_hex(f[0], 32) || !is_lower_hex(f[1], 32) || !is_short_key(f[2], 8) || !valid_provider(f[3]) ||
        !valid_event_kind(f[4]) || !parse_u64(f[5], &occurred)) {
        send_error("INVALID_ARG");
        return;
    }
    if (!same_epoch(app, f[0])) {
        send_error("BAD_STATE");
        return;
    }
    for (int i = 0; i < AM_EVENT_CACHE_SIZE; ++i) {
        if (strcmp(app->event_ids[i], f[1]) == 0) {
            (void)write_line("OK");
            return;
        }
    }
    memcpy(app->event_ids[app->next_event_id], f[1], 33);
    app->next_event_id = (app->next_event_id + 1) % AM_EVENT_CACHE_SIZE;
    memcpy(app->model.alert_key, f[2], strlen(f[2]) + 1);
    memcpy(app->model.alert_provider, f[3], strlen(f[3]) + 1);
    memcpy(app->model.alert_kind, f[4], strlen(f[4]) + 1);
    app->model.alert_until_us = now_us() + 4000000LL;
    app->model.revision++;
    (void)write_line("OK");
}

static void handle_time(am_app_t *app, char *argument)
{
    uint64_t milliseconds;
    if (!parse_u64(argument, &milliseconds) || milliseconds == 0 || milliseconds > INT64_MAX) {
        send_error("INVALID_ARG");
        return;
    }
    struct timeval value = {.tv_sec = (time_t)(milliseconds / 1000), .tv_usec = (suseconds_t)((milliseconds % 1000) * 1000)};
    if (settimeofday(&value, NULL) != 0) {
        send_error("TIME_ERROR");
        return;
    }
    app->model.clock_valid = true;
    app->model.last_clock_sync_us = now_us();
    if (!app->model.sync_active && app->model.epoch[0]) {
        app->model.link = AM_LINK_ONLINE;
    }
    app->model.revision++;
    struct timeval current;
    if (gettimeofday(&current, NULL) != 0) {
        send_error("TIME_ERROR");
        return;
    }
    (void)write_line("+TIME:%llu,%llu", (unsigned long long)current.tv_sec * 1000 + current.tv_usec / 1000, (unsigned long long)(now_us() / 1000));
    (void)write_line("OK");
}

static void handle_time_query(am_app_t *app)
{
    if (!app->model.clock_valid) {
        (void)write_line("+TIME:0,%llu,-", (unsigned long long)(now_us() / 1000));
    } else {
        struct timeval current;
        if (gettimeofday(&current, NULL) != 0) {
            send_error("TIME_ERROR");
            return;
        }
        (void)write_line("+TIME:%llu,%llu,%llu", (unsigned long long)current.tv_sec * 1000 + current.tv_usec / 1000, (unsigned long long)(now_us() / 1000),
                         (unsigned long long)((now_us() - app->model.last_clock_sync_us) / 1000));
    }
    (void)write_line("OK");
}

static void handle_setting(am_app_t *app, char *argument, const char *name)
{
    uint64_t value;
    bool rotation = strcmp(name, "TFTROTATE") == 0;
    if (!parse_u64(argument, &value) || (rotation ? (value != 0 && value != 90 && value != 180 && value != 270) : value > 255)) {
        send_error("INVALID_ARG");
        return;
    }
    xSemaphoreTake(app->display_lock, portMAX_DELAY);
    uint16_t previous = rotation ? app->rotation : strcmp(name, "TFTBRIGHT") == 0 ? app->tft_brightness : app->led_brightness;
    bool storage_error = false;
    esp_err_t err = rotation                         ? am_ui_apply_rotation(app, (uint16_t)value)
                    : strcmp(name, "TFTBRIGHT") == 0 ? am_ui_apply_tft_brightness(app, (uint8_t)value)
                                                     : am_ui_apply_led_brightness(app, (uint8_t)value);
    if (err == ESP_OK) {
        err = save_setting(app,
                           rotation                         ? "rotation"
                           : strcmp(name, "TFTBRIGHT") == 0 ? "tft_bright"
                                                            : "led_bright",
                           rotation ? (uint8_t)(value / 90) : (uint8_t)value);
        if (err != ESP_OK) {
            storage_error = true;
            if (rotation) {
                (void)am_ui_apply_rotation(app, previous);
            } else if (strcmp(name, "TFTBRIGHT") == 0) {
                (void)am_ui_apply_tft_brightness(app, previous);
            } else {
                (void)am_ui_apply_led_brightness(app, previous);
            }
        } else {
            app->model.revision++;
        }
    }
    xSemaphoreGive(app->display_lock);
    if (err != ESP_OK) {
        send_error(storage_error ? "STORAGE_ERROR" : err == ESP_ERR_NOT_SUPPORTED ? "UNSUPPORTED_FEATURE" : "INTERNAL_ERROR");
    } else {
        (void)write_line("OK");
    }
}

static void handle_mode(am_app_t *app, char *argument)
{
    bool factory;
    if (strcmp(argument, "FACTORY") == 0) {
        factory = true;
    } else if (strcmp(argument, "NORMAL") == 0) {
        factory = false;
    } else {
        send_error("INVALID_ARG");
        return;
    }
    if (factory == app->model.factory_mode) {
        (void)write_line("OK");
        return;
    }
    if (save_setting(app, "factory_mode", factory) != ESP_OK) {
        send_error("STORAGE_ERROR");
        return;
    }
    (void)write_line("OK");
    app->restart_pending = true;
}

static void handle_test(am_app_t *app, const char *command, char *argument)
{
    if (!app->model.factory_mode) {
        send_error("UNSUPPORTED_FEATURE");
        return;
    }
    if (strcmp(command, "TESTLED") == 0) {
        char *f[5];
        uint64_t index, red, green, blue, brightness;
        if (split_csv(argument, f, 5) != 5 ||
            (strcmp(f[0], "ALL") != 0 && (!parse_bounded(f[0], 15, &index))) ||
            !parse_bounded(f[1], 255, &red) || !parse_bounded(f[2], 255, &green) ||
            !parse_bounded(f[3], 255, &blue) || !parse_bounded(f[4], 255, &brightness)) {
            send_error("INVALID_ARG");
            return;
        }
        app->model.test_led_index = strcmp(f[0], "ALL") == 0 ? 16 : (uint8_t)index;
        app->model.test_led_color = (rgb_led_color_t){.r = red, .g = green, .b = blue};
        app->model.test_led_brightness = brightness;
        app->model.test = AM_TEST_LED;
    } else if (strcmp(command, "TESTTFT") == 0) {
        static const char *const s_names[] = {"BLACK", "WHITE", "RED", "GREEN", "BLUE", "GRAY"};
        static const uint16_t s_colors[] = {TFT_COLOR_BLACK, TFT_COLOR_WHITE, TFT_COLOR_RED, TFT_COLOR_GREEN, TFT_COLOR_BLUE,
                                            TFT_RGB565(128, 128, 128)};
        bool found = false;
        for (size_t i = 0; i < sizeof(s_names) / sizeof(s_names[0]); ++i) {
            if (strcmp(argument, s_names[i]) == 0) {
                app->model.test_color = s_colors[i];
                found = true;
                break;
            }
        }
        if (!found) {
            send_error("INVALID_ARG");
            return;
        }
        app->model.test = AM_TEST_COLOR;
    } else if (strcmp(command, "TESTBACKLIGHT") == 0) {
        uint64_t brightness;
        if (!parse_bounded(argument, 255, &brightness)) {
            send_error("INVALID_ARG");
            return;
        }
        app->model.test_backlight = brightness;
        app->model.test = AM_TEST_BACKLIGHT;
    } else if (strcmp(command, "TESTFONT") == 0) {
        static const char *const s_names[] = {"8x8", "8x16", "10x18", "12x22", "16x32"};
        bool found = false;
        for (size_t i = 0; i < sizeof(s_names) / sizeof(s_names[0]); ++i) {
            if (strcmp(argument, s_names[i]) == 0) {
                app->model.test_font = i;
                found = true;
                break;
            }
        }
        if (!found) {
            send_error("INVALID_ARG");
            return;
        }
        app->model.test = AM_TEST_FONT;
    } else if (strcmp(command, "TESTAUTO") == 0) {
        if (strcmp(argument, "1") == 0) {
            app->model.test = AM_TEST_AUTO;
        } else if (strcmp(argument, "0") == 0) {
            app->model.test = AM_TEST_PAGE;
        } else {
            send_error("INVALID_ARG");
            return;
        }
    } else {
        send_error("UNKNOWN_COMMAND");
        return;
    }
    app->model.test_started_us = now_us();
    app->model.revision++;
    (void)write_line("OK");
    queue_log("factory test changed");
}

static void dispatch(am_app_t *app, char *line)
{
    s_last_command_failed = false;
    s_log_failed = false;
    if (strncmp(line, "AT+", 3) != 0) {
        send_error("UNKNOWN_COMMAND");
        return;
    }
    char *command = line + 3;
    char *argument = strchr(command, '=');
    bool query = false;
    if (argument) {
        *argument++ = 0;
    } else {
        size_t length = strlen(command);
        if (length && command[length - 1] == '?') {
            command[length - 1] = 0;
            query = true;
        }
    }
    queue_log(command);
    if (strcmp(command, "HELLO") == 0 && argument && !query) {
        handle_hello(app, argument);
        return;
    }
    if (strcmp(command, "INFO") == 0 && query) {
        (void)write_line("+INFO:%s,%s,%s,%s,%d", app->device_id, AM_PRODUCT_MODEL, AM_SOFTWARE_VERSION, AM_PROTOCOL_VERSION_TEXT,
                         AM_MAX_SESSIONS);
        (void)write_line("OK");
        return;
    }
    if (strcmp(command, "MODE") == 0 && query) {
        (void)write_line("+MODE:%s", app->model.factory_mode ? "FACTORY" : "NORMAL");
        (void)write_line("OK");
        return;
    }
    if (strcmp(command, "MODE") == 0 && argument) {
        handle_mode(app, argument);
        return;
    }
    if (strcmp(command, "ECHO") == 0 || strcmp(command, "LOG") == 0) {
        bool *enabled = strcmp(command, "ECHO") == 0 ? &app->echo_enabled : &app->log_enabled;
        if (query) {
            (void)write_line("+%s:%d", command, *enabled);
            (void)write_line("OK");
        } else if (argument && (strcmp(argument, "0") == 0 || strcmp(argument, "1") == 0)) {
            *enabled = strcmp(argument, "1") == 0;
            (void)write_line("OK");
        } else {
            send_error("INVALID_ARG");
        }
        return;
    }
    if (strncmp(command, "TEST", 4) == 0) {
        if (argument && !query) {
            handle_test(app, command, argument);
        } else {
            send_error("INVALID_ARG");
        }
        return;
    }
    if (strcmp(command, "PAIRBEGIN") == 0 && !argument && !query) {
        send_error("UNSUPPORTED_FEATURE");
        return;
    }
    if (strcmp(command, "PAIRCOMMIT") == 0 && argument) {
        send_error("UNSUPPORTED_FEATURE");
        return;
    }
    if (strcmp(command, "AUTH") == 0 && argument) {
        send_error("UNSUPPORTED_FEATURE");
        return;
    }
    if (app->model.factory_mode) {
        send_error("UNSUPPORTED_FEATURE");
        return;
    }
    if (strcmp(command, "SYNCBEGIN") == 0 && argument) {
        handle_sync_begin(app, argument);
    } else if (strcmp(command, "SESSION") == 0 && argument) {
        handle_session(app, argument);
    } else if (strcmp(command, "DELETE") == 0 && argument) {
        handle_delete(app, argument);
    } else if (strcmp(command, "SYNCEND") == 0 && argument) {
        handle_sync_end(app, argument);
    } else if (strcmp(command, "LIST") == 0 && query) {
        handle_list(app);
    } else if (strcmp(command, "GET") == 0 && argument) {
        handle_get(app, argument);
    } else if (strcmp(command, "USAGE") == 0 && argument) {
        handle_usage(app, argument);
    } else if (strcmp(command, "USAGECLEAR") == 0 && argument) {
        handle_usage_clear(app, argument);
    } else if (strcmp(command, "EVENT") == 0 && argument) {
        handle_event(app, argument);
    } else if (strcmp(command, "PING") == 0 && argument) {
        if (!valid_nonce(argument)) {
            send_error("INVALID_ARG");
        } else {
            (void)write_line("+PONG:%s,%llu", argument, (unsigned long long)(now_us() / 1000));
            (void)write_line("OK");
        }
    } else if (strcmp(command, "TIME") == 0 && argument) {
        handle_time(app, argument);
    } else if (strcmp(command, "TIME") == 0 && query) {
        handle_time_query(app);
    } else if (strcmp(command, "TFTBRIGHT") == 0 && argument) {
        handle_setting(app, argument, command);
    } else if (strcmp(command, "LEDBRIGHT") == 0 && argument) {
        handle_setting(app, argument, command);
    } else if (strcmp(command, "TFTROTATE") == 0 && argument) {
        handle_setting(app, argument, command);
    } else if ((strcmp(command, "TFTBRIGHT") == 0 || strcmp(command, "LEDBRIGHT") == 0 || strcmp(command, "TFTROTATE") == 0) && query) {
        (void)write_line("+%s:%u", command,
                         strcmp(command, "TFTBRIGHT") == 0   ? app->tft_brightness
                         : strcmp(command, "LEDBRIGHT") == 0 ? app->led_brightness
                                                             : app->rotation);
        (void)write_line("OK");
    } else {
        send_error("UNKNOWN_COMMAND");
    }
}

void am_protocol_run(am_app_t *app)
{
    uart_config_t config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    ESP_ERROR_CHECK(uart_driver_install(UART_NUM_0, 1024, 1024, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(UART_NUM_0, &config));
    ESP_ERROR_CHECK(uart_set_pin(UART_NUM_0, GPIO_NUM_21, GPIO_NUM_20, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    char line[AM_LINE_BYTES + 1];
    size_t length = 0;
    bool overflow = false;
    bool skip_lf = false;
    for (;;) {
        int64_t now = now_us();
        xSemaphoreTake(app->model_lock, portMAX_DELAY);
        if (!app->model.factory_mode && app->last_valid_command_us > 0 && now - app->last_valid_command_us > AM_CONNECTION_TIMEOUT_US &&
            app->model.link != AM_LINK_WAIT_HOST) {
            clear_remote(app);
            app->model.link = AM_LINK_WAIT_HOST;
            app->last_valid_command_us = 0;
            queue_log("host timeout");
        }
        xSemaphoreGive(app->model_lock);
        uint8_t data[64];
        int received = uart_read_bytes(UART_NUM_0, data, sizeof(data), pdMS_TO_TICKS(100));
        for (int i = 0; i < received; ++i) {
            char ch = (char)data[i];
            if (ch == '\n' && skip_lf) {
                skip_lf = false;
                continue;
            }
            skip_lf = ch == '\r';
            if (ch == '\r' || ch == '\n') {
                if (app->echo_enabled) {
                    (void)uart_write_bytes(UART_NUM_0, "\r\n", 2);
                }
                if (overflow) {
                    send_error("LINE_TOO_LONG");
                } else {
                    line[length] = 0;
                    if (length) {
                        xSemaphoreTake(app->model_lock, portMAX_DELAY);
                        dispatch(app, line);
                        if (!app->model.factory_mode && !s_last_command_failed) {
                            app->last_valid_command_us = now_us();
                        }
                        xSemaphoreGive(app->model_lock);
                        flush_log(app);
                        if (app->restart_pending) {
                            (void)uart_wait_tx_done(UART_NUM_0, pdMS_TO_TICKS(500));
                            esp_restart();
                        }
                    }
                }
                length = 0;
                overflow = false;
            } else if (ch == '\b' || ch == 0x7f) {
                if (length > 0 && !overflow) {
                    --length;
                    if (app->echo_enabled) {
                        (void)uart_write_bytes(UART_NUM_0, "\b \b", 3);
                    }
                }
            } else if (!overflow) {
                if (length >= AM_LINE_BYTES - 2) {
                    overflow = true;
                } else {
                    line[length++] = ch;
                    if (app->echo_enabled) {
                        (void)uart_write_bytes(UART_NUM_0, &ch, 1);
                    }
                }
            }
        }
        flush_log(app);
    }
}
