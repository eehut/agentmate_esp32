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
#
# Date       Version  Author       Description
# ---------- -------- ------------ -----------------------------------------------
# 2026-10-01 1.0      Liu Chuansen  Organized for project C style.
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

#define AM_MAX_SESSIONS 4
#define AM_MAX_USAGE_ENTRIES 8
#define AM_EVENT_CACHE_SIZE 32
#define AM_LINE_BYTES 256

typedef enum {
    AM_LINK_WAIT_HOST,
    AM_LINK_HANDSHAKE,
    AM_LINK_PAIRING,
    AM_LINK_UNPAIRED,
    AM_LINK_SYNCING,
    AM_LINK_ONLINE,
    AM_LINK_DEGRADED,
} am_link_t;

typedef struct {
    bool used;
    bool seen;
    char key[9];
    uint64_t rev;
    char provider[17];
    char state[24];
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
    char window[9];
    int32_t used_bp; /* -1 means unknown. */
    uint64_t reset_at;
    uint32_t window_seconds;
    uint64_t updated_at;
} am_usage_t;

typedef struct {
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
    char alert_kind[24];
    char alert_key[9];
    char alert_provider[17];
    int64_t alert_until_us;
    bool clock_valid;
    int64_t last_clock_sync_us;
    uint32_t revision;
} am_model_t;

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
    uint8_t tft_brightness;
    uint8_t led_brightness;
    uint16_t rotation;
    am_model_t model;
} am_app_t;

esp_err_t am_app_init(am_app_t *app);
void am_protocol_run(am_app_t *app);
void am_ui_run(am_app_t *app);

/* The caller holds display_lock while changing display settings. */
esp_err_t am_ui_apply_rotation(am_app_t *app, uint16_t degrees);
esp_err_t am_ui_apply_tft_brightness(am_app_t *app, uint8_t value);
esp_err_t am_ui_apply_led_brightness(am_app_t *app, uint8_t value);

#endif /* AGENTMATE_APP_H */
