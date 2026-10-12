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
# Description: AgentMate application shared data and interfaces
# Author: Liu Chuansen <liuchuansen@outlook.com>
#
#####################################################################################
#
# Revision History:
# 2026-10-04 3.0      Codex         Add protocol 2 host labels and session display names.
#
# Date       Version  Author       Description
# ---------- -------- ------------ -----------------------------------------------
# 2026-10-01 1.0      Liu Chuansen  Organized for project C style.
# 2026-10-01 1.1      Codex         Added the protocol version constant.
# 2026-10-01 1.2      Codex         Defined the product model.
# 2026-10-02 1.3      Codex         Added phase-one UI state and local-time fields.
# 2026-10-03 1.4      Codex         Shared percentage brightness conversion.
# 2026-10-03 1.9      Codex         Expose successfully rendered status animation frames.
# 2026-10-03 1.8      Codex         Expose selected session and submitted LED frame diagnostics.
# 2026-10-03 1.7      Codex         Default to eight sessions and four usage records.
# 2026-10-03 1.6      Codex         Track connection-local usage display support.
# 2026-10-03 1.5      Codex         Added bounded atomic usage batch storage.
# 2026-10-03 1.10     Codex         Added deterministic LED v2 patterns and frame diagnostics.
#
#####################################################################################
*/

#ifndef AGENTMATE_APP_H
#define AGENTMATE_APP_H

#include <stdbool.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "nvs.h"
#include "rgb_led_strip.h"
#include "tft_display.h"

#define AM_MAX_SESSIONS 8
#define AM_MAX_USAGE_ENTRIES 4
#define AM_EVENT_CACHE_SIZE 32
#define AM_LINE_BYTES 512
#define AM_PROJECT_BYTES 64
#define AM_PROTOCOL_VERSION_TEXT "2"
#define AM_PRODUCT_MODEL "AGM-C3"

#ifndef AM_SOFTWARE_VERSION
#error "AM_SOFTWARE_VERSION must be provided by the project version at build time"
#endif

typedef enum {
    AM_LINK_WAIT_HOST,
    AM_LINK_HANDSHAKE,
    AM_LINK_PAIRING,
    AM_LINK_UNPAIRED,
    AM_LINK_SYNCING,
    AM_LINK_ONLINE,
    AM_LINK_DEGRADED,
} am_link_t;

typedef enum {
    AM_TEST_PAGE,
    AM_TEST_LED,
    AM_TEST_COLOR,
    AM_TEST_BACKLIGHT,
    AM_TEST_FONT,
    AM_TEST_AUTO,
} am_test_t;

typedef struct {
    bool used;
    bool seen;
    char key[9];
    uint64_t rev;
    char provider[17];
    char host_label[9];
    char session_short_name[12];
    char state[24];
    char project_name[AM_PROJECT_BYTES + 1];
    char last_task[10];
    uint64_t finished_at;
    uint32_t task_seconds;
    int64_t task_base_us;
    /* 0: unsupported, 1: known but unavailable, 2: known. */
    uint8_t context_mode;
    uint64_t ctx_used;
    uint64_t ctx_window;
    uint16_t ctx_pct_bp;
    uint64_t ctx_updated_at;
} am_session_t;

typedef struct {
    bool used;
    char provider[17];
    char metric[9];
    char window[17];
    int32_t used_bp; /* -1 means unknown. */
    uint64_t reset_at;
    uint32_t window_seconds;
    uint64_t updated_at;
    bool reset_offset_valid;
    int16_t reset_utc_offset_minutes;
} am_usage_t;

/* Persistent IDs: append new themes without reordering existing entries. */
enum am_clock_theme {
    AM_CLOCK_AMBER = 0,
    AM_CLOCK_WHITE,
    AM_CLOCK_MINT,
    AM_CLOCK_CODEX,
    AM_CLOCK_THEME_COUNT,
};

typedef struct {
    uint8_t clock_theme;
    bool factory_mode;
    am_test_t test;
    uint8_t test_led_index;
    rgb_led_color_t test_led_color;
    uint8_t test_led_brightness;
    uint8_t test_backlight;
    uint16_t test_color;
    uint8_t test_font;
    int64_t test_started_us;
    am_link_t link;
    char epoch[33];
    bool sync_active;
    uint64_t sync_id;
    uint64_t last_sync_end_id;
    int session_count;
    am_session_t sessions[AM_MAX_SESSIONS];
    int usage_count;
    am_usage_t usages[AM_MAX_USAGE_ENTRIES];
    char pair_pin[5];
    int64_t pair_deadline_us;
    int64_t pair_cooldown_us;
    char alert_key[9];
    char alert_provider[17];
    uint32_t attention_sequence;
    char attention_key[9];
    int64_t info_until_us;
    uint32_t info_sequence;
    bool usage_supported;
    uint32_t remote_generation;
    bool clock_valid;
    bool utc_offset_valid;
    int16_t utc_offset_minutes;
    bool protocol_ready;
    int64_t last_clock_sync_us;
    uint32_t revision;
} am_model_t;

struct am_usage_batch {
    bool active;
    bool failed;
    char provider[17];
    uint64_t id;
    int64_t started_us;
    char last_provider[17];
    uint64_t last_id;
    am_usage_t entries[AM_MAX_USAGE_ENTRIES];
};

typedef struct {
    SemaphoreHandle_t model_lock;
    SemaphoreHandle_t display_lock;
    nvs_handle_t nvs;
    tft_display_handle_t display;
    rgb_led_strip_handle_t strip;
    char device_id[33];
    char owner_server_id[33];
    uint8_t device_key[32];
    bool paired;
    char server_id[33];
    char server_nonce[33];
    char device_nonce[33];
    bool hello_valid;
    bool authenticated;
    char pair_id[33];
    uint8_t pair_attempts;
    int64_t last_valid_command_us;
    char event_ids[AM_EVENT_CACHE_SIZE][33];
    uint8_t next_event_id;
    /* Driver and NVS brightness remain 0..255; public settings use 0..100 percent. */
    uint8_t tft_brightness;
    uint8_t led_brightness;
    uint16_t rotation;
    bool echo_enabled;
    bool log_enabled;
    bool restart_pending;
    uint32_t ui_stack_free;
    uint32_t ui_render_max_us;
    uint8_t ui_page;
    uint8_t ui_group;
    uint8_t ui_slot;
    uint8_t ui_led_mode;
    uint16_t ui_led_mask;
    uint16_t ui_led_sum;
    uint32_t ui_led_pixel0;
    uint32_t ui_led_pixel8;
    uint32_t ui_led_rgb;
    uint32_t ui_page_sequence;
    uint32_t ui_status_sequence;
    uint8_t ui_status_kind;
    uint8_t ui_status_frame;
    struct am_usage_batch usage_batch;
    am_model_t model;
} am_app_t;

/* Callers validate percent <= 100 before converting to driver brightness. */
static inline uint8_t am_brightness_from_percent(uint8_t percent)
{
    return (uint8_t)((percent * 255U + 50U) / 100U);
}

static inline uint8_t am_brightness_to_percent(uint8_t value)
{
    return (uint8_t)((value * 100U + 127U) / 255U);
}

esp_err_t am_app_init(am_app_t *app);
void am_protocol_run(am_app_t *app);
void am_ui_run(am_app_t *app);

/* The caller holds display_lock while changing display settings. */
esp_err_t am_ui_apply_rotation(am_app_t *app, uint16_t degrees);
esp_err_t am_ui_apply_tft_brightness(am_app_t *app, uint8_t value);
esp_err_t am_ui_apply_led_brightness(am_app_t *app, uint8_t value);

#endif /* AGENTMATE_APP_H */
