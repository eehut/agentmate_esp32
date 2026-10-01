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
# Description: AgentMate TFT and RGB LED user interface
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

#include "agentmate_app.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#include "esp_timer.h"
#include "freertos/task.h"
#include "lcd_fonts.h"
#include "provider_icons.h"

#define BG TFT_RGB565(12, 17, 28)
#define FG TFT_RGB565(234, 240, 249)
#define MUTED TFT_RGB565(125, 143, 163)
#define ACCENT TFT_RGB565(84, 203, 190)
#define WARNING TFT_RGB565(255, 185, 72)
#define DANGER TFT_RGB565(255, 91, 112)

static uint16_t s_text_pixels[224 * 16];

static esp_err_t draw_text(am_app_t *app, int x, int y, const char *value, uint16_t foreground, uint16_t background)
{
    size_t count = strlen(value);
    if (count > 28) {
        count = 28;
    }
    if (!count) {
        return ESP_OK;
    }
    const lcd_font_t *font = &g_lcd_font_ascii_8x16;
    for (size_t c = 0; c < count; ++c) {
        unsigned char ch = (unsigned char)value[c];
        const uint8_t *glyph = font->get_code_data(font, ch >= 32 && ch <= 126 ? ch : '?');
        for (int row = 0; row < 16; ++row) {
            uint8_t bits = glyph ? glyph[row] : 0;
            for (int column = 0; column < 8; ++column) {
                s_text_pixels[row * count * 8 + c * 8 + column] = (bits & (0x80U >> column)) ? foreground : background;
            }
        }
    }
    return tft_display_draw_bitmap_rgb565(app->display, x, y, (int)count * 8, 16, s_text_pixels);
}

static void label(am_app_t *app, int x, int y, const char *value, uint16_t color)
{
    (void)draw_text(app, x, y, value, color, BG);
}

static void header(am_app_t *app, const char *state, const char *clock)
{
    label(app, 18, 18, "AGENTMATE", ACCENT);
    label(app, 18, 37, state, MUTED);
    label(app, 18, 210, clock, MUTED);
}

static void clock_text(const am_model_t *model, char *out, size_t capacity)
{
    if (!model->clock_valid) {
        snprintf(out, capacity, "TIME --:-- UTC");
        return;
    }
    time_t seconds = time(NULL);
    struct tm utc;
    if (gmtime_r(&seconds, &utc) == NULL) {
        snprintf(out, capacity, "TIME --:-- UTC");
        return;
    }
    snprintf(out, capacity, "%02d:%02d:%02d UTC", utc.tm_hour, utc.tm_min, utc.tm_sec);
}

static bool advancing(const char *state)
{
    return strcmp(state, "working") == 0 || strcmp(state, "waiting_for_input") == 0 || strcmp(state, "permission_required") == 0;
}

static void session_page(am_app_t *app, const am_model_t *model, const am_session_t *session, const char *clock, int page, int total)
{
    char buffer[64];
    header(app, "CONNECTED", clock);
    const uint16_t *icon = strcmp(session->provider, "claude") == 0 ? s_am_icon_claude : s_am_icon_codex;
    (void)tft_display_draw_bitmap_rgb565(app->display, 18, 66, AM_PROVIDER_ICON_SIZE, AM_PROVIDER_ICON_SIZE, icon);
    snprintf(buffer, sizeof(buffer), "%s  %d/%d", session->provider, page, total);
    label(app, 77, 68, buffer, FG);
    snprintf(buffer, sizeof(buffer), "SESSION %s", session->key);
    label(app, 77, 89, buffer, MUTED);
    label(app, 18, 125, session->state, strcmp(session->state, "permission_required") == 0 ? WARNING : strcmp(session->state, "working") == 0 ? ACCENT : FG);
    uint64_t duration = session->task_seconds;
    if (advancing(session->state) && session->task_base_us > 0) {
        duration += (esp_timer_get_time() - session->task_base_us) / 1000000;
    }
    snprintf(buffer, sizeof(buffer), "TASK %llum %02llus", (unsigned long long)(duration / 60), (unsigned long long)(duration % 60));
    label(app, 18, 147, buffer, FG);
    if (session->context_mode == 2) {
        snprintf(buffer, sizeof(buffer), "CTX %u.%02u%%", session->ctx_pct_bp / 100, session->ctx_pct_bp % 100);
        label(app, 18, 169, buffer, ACCENT);
        snprintf(buffer, sizeof(buffer), "TOK %lluk/%lluk", (unsigned long long)(session->ctx_used / 1000), (unsigned long long)(session->ctx_window / 1000));
        label(app, 18, 190, buffer, MUTED);
    } else if (session->context_mode == 1) {
        label(app, 18, 169, "CTX UNKNOWN", MUTED);
    } else {
        label(app, 18, 169, "CTX N/A", MUTED);
    }
    if (session->context_mode != 2 && strcmp(session->last_task, "-") != 0) {
        snprintf(buffer, sizeof(buffer), "LAST %s", session->last_task);
        label(app, 18, 190, buffer, MUTED);
    }
    (void)model;
}

static void usage_page(am_app_t *app, const am_model_t *model, const char *clock)
{
    header(app, "PROVIDER USAGE", clock);
    int shown = 0;
    for (int i = 0; i < AM_MAX_USAGE_ENTRIES && shown < 4; ++i) {
        const am_usage_t *usage = &model->usages[i];
        if (!usage->used) {
            continue;
        }
        char buffer[32];
        snprintf(buffer, sizeof(buffer), "%s %s", usage->provider, usage->window);
        label(app, 18, 70 + shown * 32, buffer, FG);
        if (usage->used_bp >= 0) {
            snprintf(buffer, sizeof(buffer), "%s %d.%02d%%", usage->metric, (int)(usage->used_bp / 100), (int)(usage->used_bp % 100));
        } else {
            snprintf(buffer, sizeof(buffer), "%s UNKNOWN", usage->metric);
        }
        label(app, 18, 86 + shown * 32, buffer, ACCENT);
        shown++;
    }
    if (!shown) {
        label(app, 18, 100, "NO USAGE DATA", MUTED);
    }
}

static void status_page(am_app_t *app, const am_model_t *model, const char *clock, const char *device_id)
{
    header(app, model->link == AM_LINK_DEGRADED ? "TIME UNSYNCED" : "CONNECTED", clock);
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "SESSIONS %d/%d", model->session_count, AM_MAX_SESSIONS);
    label(app, 18, 76, buffer, FG);
    snprintf(buffer, sizeof(buffer), "USAGE %d/%d", model->usage_count, AM_MAX_USAGE_ENTRIES);
    label(app, 18, 100, buffer, FG);
    snprintf(buffer, sizeof(buffer), "DEVICE %.8s", device_id + 24);
    label(app, 18, 124, buffer, MUTED);
    label(app, 18, 145, "FW 1.0", MUTED);
    if (model->clock_valid) {
        snprintf(buffer, sizeof(buffer), "SYNC AGE %llus", (unsigned long long)((esp_timer_get_time() - model->last_clock_sync_us) / 1000000));
    } else {
        snprintf(buffer, sizeof(buffer), "CLOCK WAITING");
    }
    label(app, 18, 166, buffer, ACCENT);
    snprintf(buffer, sizeof(buffer), "UPTIME %llum", (unsigned long long)(esp_timer_get_time() / 60000000));
    label(app, 18, 188, buffer, MUTED);
}

static void pairing_page(am_app_t *app, const am_model_t *model, const char *clock, const char *device_id)
{
    header(app, "PAIRING", clock);
    label(app, 72, 71, model->pair_pin, WARNING);
    label(app, 32, 105, "ENTER CODE IN WEB", FG);
    char buffer[32];
    int remaining = (int)((model->pair_deadline_us - esp_timer_get_time() + 999999) / 1000000);
    if (remaining < 0) {
        remaining = 0;
    }
    snprintf(buffer, sizeof(buffer), "EXPIRES IN %ds", remaining);
    label(app, 48, 133, buffer, MUTED);
    snprintf(buffer, sizeof(buffer), "DEVICE %.8s", device_id + 24);
    label(app, 48, 162, buffer, MUTED);
}

static rgb_led_color_t led_color(const am_model_t *model)
{
    if (model->link == AM_LINK_PAIRING) {
        return RGB_LED_COLOR_BLUE;
    }
    if (model->link != AM_LINK_ONLINE && model->link != AM_LINK_DEGRADED) {
        return (rgb_led_color_t){.r = 20, .g = 25, .b = 40};
    }
    for (int i = 0; i < AM_MAX_SESSIONS; ++i) {
        if (model->sessions[i].used && strcmp(model->sessions[i].state, "permission_required") == 0) {
            return RGB_LED_COLOR_RED;
        }
    }
    for (int i = 0; i < AM_MAX_SESSIONS; ++i) {
        if (model->sessions[i].used && strcmp(model->sessions[i].state, "working") == 0) {
            return RGB_LED_COLOR_GREEN;
        }
    }
    return model->link == AM_LINK_DEGRADED ? RGB_LED_COLOR_RED : RGB_LED_COLOR_BLUE;
}

esp_err_t am_ui_apply_rotation(am_app_t *app, uint16_t degrees)
{
    esp_err_t err = tft_display_set_rotation(app->display, (tft_rotation_t)(degrees / 90));
    if (err == ESP_OK) {
        app->rotation = degrees;
    }
    return err;
}

esp_err_t am_ui_apply_tft_brightness(am_app_t *app, uint8_t value)
{
    esp_err_t err = tft_display_set_brightness(app->display, value);
    if (err == ESP_OK) {
        app->tft_brightness = value;
    }
    return err;
}

esp_err_t am_ui_apply_led_brightness(am_app_t *app, uint8_t value)
{
    esp_err_t err = rgb_led_strip_set_brightness(app->strip, value);
    if (err == ESP_OK) {
        err = rgb_led_strip_show(app->strip);
    }
    if (err == ESP_OK) {
        app->led_brightness = value;
    }
    return err;
}

void am_ui_run(am_app_t *app)
{
    static am_model_t s_snapshot;
    uint32_t previous_revision = UINT32_MAX;
    int64_t previous_second = -1;
    int previous_page = -1;
    uint16_t previous_rotation = UINT16_MAX;
    char device_id[33];
    memcpy(device_id, app->device_id, sizeof(device_id));
    for (;;) {
        xSemaphoreTake(app->model_lock, portMAX_DELAY);
        memcpy(&s_snapshot, &app->model, sizeof(s_snapshot));
        uint16_t rotation = app->rotation;
        xSemaphoreGive(app->model_lock);
        int64_t now = esp_timer_get_time();
        int page = (int)((now / 6000000) % (s_snapshot.session_count + 2));
        if (s_snapshot.link == AM_LINK_PAIRING || s_snapshot.link == AM_LINK_WAIT_HOST || s_snapshot.link == AM_LINK_HANDSHAKE ||
            s_snapshot.link == AM_LINK_UNPAIRED || s_snapshot.link == AM_LINK_SYNCING) {
            page = -1;
        }
        if (s_snapshot.alert_kind[0] && now < s_snapshot.alert_until_us) {
            int ordinal = 0;
            for (int i = 0; i < AM_MAX_SESSIONS; ++i) {
                if (s_snapshot.sessions[i].used) {
                    if (strcmp(s_snapshot.sessions[i].key, s_snapshot.alert_key) == 0) {
                        page = ordinal;
                        break;
                    }
                    ordinal++;
                }
            }
        }
        if (s_snapshot.link == AM_LINK_ONLINE || s_snapshot.link == AM_LINK_DEGRADED) {
            if (s_snapshot.alert_kind[0] == 0 || now >= s_snapshot.alert_until_us) {
                int ordinal = 0;
                for (int i = 0; i < AM_MAX_SESSIONS; ++i) {
                    if (!s_snapshot.sessions[i].used) {
                        continue;
                    }
                    if (strcmp(s_snapshot.sessions[i].state, "permission_required") == 0 || strcmp(s_snapshot.sessions[i].state, "waiting_for_input") == 0) {
                        page = ordinal;
                        break;
                    }
                    ordinal++;
                }
            }
            if (page == s_snapshot.session_count && s_snapshot.usage_count == 0) {
                page = s_snapshot.session_count + 1;
            }
        }
        int64_t second = now / 1000000;
        if (s_snapshot.revision != previous_revision || second != previous_second || page != previous_page || rotation != previous_rotation) {
            char clock[24];
            clock_text(&s_snapshot, clock, sizeof(clock));
            xSemaphoreTake(app->display_lock, portMAX_DELAY);
            (void)tft_display_fill(app->display, BG);
            if (s_snapshot.link == AM_LINK_PAIRING && s_snapshot.pair_pin[0]) {
                pairing_page(app, &s_snapshot, clock, device_id);
            } else if (s_snapshot.link != AM_LINK_ONLINE && s_snapshot.link != AM_LINK_DEGRADED) {
                header(app,
                       s_snapshot.link == AM_LINK_SYNCING     ? "SYNCING"
                       : s_snapshot.link == AM_LINK_UNPAIRED  ? "PAIR REQUIRED"
                       : s_snapshot.link == AM_LINK_HANDSHAKE ? "HANDSHAKE"
                                                              : "WAIT HOST",
                       clock);
                label(app, 18, 90, "USB SERIAL READY", FG);
                label(app, 18, 125, "AGENTMATE ESP32", MUTED);
            } else if (page < s_snapshot.session_count) {
                int ordinal = 0;
                for (int i = 0; i < AM_MAX_SESSIONS; ++i) {
                    if (!s_snapshot.sessions[i].used) {
                        continue;
                    }
                    if (ordinal == page) {
                        session_page(app, &s_snapshot, &s_snapshot.sessions[i], clock, ordinal + 1, s_snapshot.session_count);
                        break;
                    }
                    ordinal++;
                }
            } else if (page == s_snapshot.session_count) {
                usage_page(app, &s_snapshot, clock);
            } else {
                status_page(app, &s_snapshot, clock, device_id);
            }
            (void)rgb_led_strip_fill(app->strip, led_color(&s_snapshot));
            (void)rgb_led_strip_show(app->strip);
            xSemaphoreGive(app->display_lock);
            previous_revision = s_snapshot.revision;
            previous_second = second;
            previous_page = page;
            previous_rotation = rotation;
        }
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}
