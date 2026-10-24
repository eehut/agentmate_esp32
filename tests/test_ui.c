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
# Description: Host regression checks for AgentMate UI
# Author: Liu Chuansen <liuchuansen@outlook.com>
#
#####################################################################################
#
# Revision History:
# 2026-10-25 5.0      Codex         Verify active-plus-retained-result counts through completion transitions.
# 2026-10-25 4.9      Codex         Verify static session arcs, selection glow and partial redraw cleanup.
# 2026-10-24 4.8      Codex         Verify unavailable standby quotas are skipped and recover cleanly.
# 2026-10-24 4.7      Codex         Regress completion-group dot counts against the actual rotation selection.
# 2026-10-24 4.6      Codex         Verify inactive sessions do not consume dots or shift active highlights.
# 2026-10-24 4.5      Codex         Verify centered fixed dot positions and single-session indicators.
# 2026-10-24 4.4      Codex         Verify custom RGB clock colors and derived calendar palettes.
# 2026-10-24 4.3      Codex         Check fixed dot placement at two, five, six and eight sessions.
# 2026-10-24 4.2      Codex         Verify all-session arc colors, spacing and breathing partial redraws.
# 2026-10-13 4.1      Codex         Verify five-second clock previews and attention protection.
# 2026-10-12 4.0      Codex         Verify OTA pages, safe geometry and LED progress.
# 2026-10-12 3.7      Codex         Verify v6.2 page geometry, typography and partial redraws.
# 2026-10-11 3.6      Codex         Verify valid-only metrics, 10/3/3 dwell and rounded header time.
# 2026-10-11 3.5      Codex         Verify ASCII directory substitution and black status interiors.
# 2026-10-11 3.4      Codex         Verify Session panels, rotation, token units and mapped border pixels.
# 2026-10-09 3.3      Codex         Verify Codex context rotation and incremental rendering.
# 2026-10-06 3.2      Codex         Check larger quota numbers and lowered five-hour group.
# 2026-10-06 3.1      Codex         Verify session quota progress bars and shared usage selection.
# 2026-10-04 3.0      Codex         Add protocol 2 host labels and session display names.
#
# Date       Version  Author       Description
# ---------- -------- ------------ -----------------------------------------------
# 2026-10-02 1.0      Codex         Created phase-one implementation.
# 2026-10-03 1.1      Codex         Verify percentage brightness conversion.
# 2026-10-03 1.8      Codex         Verify animated borders and export dynamic previews.
# 2026-10-04 1.9      Codex         Verify clock separator timing and bounded redraws.
# 2026-10-03 1.7      Codex         Verify priority groups and ten-second noninterrupting rotation.
# 2026-10-03 1.6      Codex         Verify eight-session rotation and four-entry quota capacity.
# 2026-10-03 1.5      Codex         Check quota color boundaries, rounding and incremental rendering.
# 2026-10-03 1.4      Codex         Verify adaptive directory sizes and Codex title contrast.
# 2026-10-03 1.3      Codex         Verify sticky usage support and five-second standby rotation.
# 2026-10-03 1.2      Codex         Verify atomic quota updates and reset layout.
#
#####################################################################################
*/

/* Host regression harness uses the firmware scene builder and renderer directly. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "agentmate_text.h"
#include "agentmate_usage.h"
#include "../main/agentmate_ui.c"

static uint16_t s_screen[240 * 240];
static rgb_led_color_t s_ring[16];
static unsigned s_submitted_pixels;
static int64_t s_now;

int64_t esp_timer_get_time(void) { return s_now; }
void test_led_patterns(void);
void write_led_preview(const char *directory);
esp_err_t tft_display_draw_bitmap_rgb565(tft_display_handle_t h, int x, int y, int w, int height, const uint16_t *p)
{
    (void)h;
    assert(x >= 0 && y >= 0 && x + w <= 240 && y + height <= 240);
    assert(w * height <= 240 * 32);
    for (int row = 0; row < height; ++row) {
        memcpy(s_screen + (y + row) * 240 + x, p + row * w, (size_t)w * sizeof(*p));
    }
    s_submitted_pixels += (unsigned)(w * height);
    return ESP_OK;
}
esp_err_t tft_display_fill(tft_display_handle_t h, uint16_t color)
{
    (void)h;
    for (int i = 0; i < 240 * 240; ++i) { s_screen[i] = color; }
    return ESP_OK;
}
esp_err_t tft_display_set_brightness(tft_display_handle_t h,uint8_t v) { (void)h; (void)v; return ESP_OK; }
esp_err_t tft_display_set_rotation(tft_display_handle_t h,tft_rotation_t v) { (void)h; (void)v; return ESP_OK; }
esp_err_t rgb_led_strip_set_brightness(rgb_led_strip_handle_t h,uint8_t v) { (void)h; (void)v; return ESP_OK; }
esp_err_t rgb_led_strip_clear(rgb_led_strip_handle_t h) { (void)h; memset(s_ring, 0, sizeof(s_ring)); return ESP_OK; }
esp_err_t rgb_led_strip_show(rgb_led_strip_handle_t h) { (void)h; return ESP_OK; }
esp_err_t rgb_led_strip_fill(rgb_led_strip_handle_t h,rgb_led_color_t v)
{
    (void)h;
    for (int i = 0; i < 16; ++i) { s_ring[i] = v; }
    return ESP_OK;
}
esp_err_t rgb_led_strip_set_pixel(rgb_led_strip_handle_t h,size_t i,rgb_led_color_t v) { (void)h; assert(i < 16); s_ring[i] = v; return ESP_OK; }
esp_err_t rgb_led_strip_set_pixels(rgb_led_strip_handle_t h,size_t i,const rgb_led_color_t *v,size_t n)
{
    (void)h; assert(i + n <= 16); memcpy(s_ring + i,v,n * sizeof(*v)); return ESP_OK;
}

static am_session_t session(const char *key, const char *state, const char *provider)
{
    am_session_t s = {.used = true, .task_seconds = 725, .task_base_us = 1000000};
    snprintf(s.key, sizeof(s.key), "%s", key);
    snprintf(s.host_label, sizeof(s.host_label), "LOCAL");
    snprintf(s.session_short_name, sizeof(s.session_short_name), "S1");
    snprintf(s.state, sizeof(s.state), "%s", state);
    snprintf(s.provider, sizeof(s.provider), "%s", provider);
    snprintf(s.project_name, sizeof(s.project_name), "agentmate_esp32");
    return s;
}

static void test_text(void)
{
    assert(am_host_label_valid("LOCAL") && am_host_label_valid("WSL") && am_host_label_valid("SERVER01"));
    assert(!am_host_label_valid("AB") && !am_host_label_valid("ABCDEFGHI") && !am_host_label_valid("wsl") && !am_host_label_valid("WSL*S1"));
    assert(am_session_short_name_valid("S1") && am_session_short_name_valid("S4294967295"));
    assert(!am_session_short_name_valid("S0") && !am_session_short_name_valid("S01") && !am_session_short_name_valid("S4294967296"));
    char name[65], encoded[193];
    assert(am_project_decode("project%2C%E4%B8%AD%E6%96%87", name));
    assert(strcmp(name, "project,中文") == 0);
    am_project_encode(name, encoded);
    assert(strcmp(encoded, "project%2C%E4%B8%AD%E6%96%87") == 0);
    const char *bad[] = {"%", "%A", "%GG", "%00", "%0A", "%2F", "%5C", "%C0%AF", "%ED%A0%80", "%F4%90%80%80", "space name"};
    for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); ++i) { assert(!am_project_decode(bad[i], name)); }
    char longest[66]; memset(longest, 'a', 65); longest[65] = 0;
    assert(!am_project_decode(longest, name)); longest[64] = 0; assert(am_project_decode(longest, name));
    int16_t offset;
    assert(am_parse_offset("-840", &offset) && offset == -840);
    assert(am_parse_offset("+480", &offset) && offset == 480);
    assert(am_parse_offset("0", &offset) && offset == 0);
    assert(!am_parse_offset("841", &offset) && !am_parse_offset("-", &offset) && !am_parse_offset("1.0", &offset));
}

static void test_selection(void)
{
    am_model_t model = {.link = AM_LINK_ONLINE};
    struct am_ui_schedule schedule = {0};
    struct am_ui_selection out;
    am_ui_select(&model, &schedule, 1000000, &out); assert(out.page == AM_PAGE_IDLE);
    model.sessions[0] = session("s1", "working", "codex");
    model.sessions[1] = session("s2", "thinking", "claude");
    model.sessions[2] = session("s3", "task_failed", "codex");
    am_ui_select(&model, &schedule, 2000000, &out);
    assert(out.page == AM_PAGE_RESULT && out.group == AM_GROUP_RESULT && out.count == 1 && out.slots[0] == 2);
    strcpy(model.sessions[0].state, "waiting_for_input");
    strcpy(model.attention_key, "s1"); model.attention_sequence++;
    am_ui_select(&model, &schedule, 3000000, &out);
    assert(out.group == AM_GROUP_ATTENTION && out.preempted && out.slots[out.current] == 0);
    strcpy(model.sessions[1].state, "permission_required");
    strcpy(model.attention_key, "s2"); model.attention_sequence++;
    am_ui_select(&model, &schedule, 5000000, &out);
    assert(out.count == 2 && out.current == 0 && !out.preempted && schedule.page_since_us == 3000000);
    strcpy(model.alert_key, "s2"); strcpy(model.alert_provider, "claude");
    assert(am_event_in_group(&model));
    am_ui_select(&model, &schedule, 12999999, &out); assert(out.current == 0);
    am_ui_select(&model, &schedule, 13000000, &out); assert(out.current == 1);
    strcpy(model.sessions[1].state, "waiting_for_input");
    am_ui_select(&model, &schedule, 15000000, &out); assert(out.current == 1 && schedule.page_since_us == 13000000);
    model.info_until_us = 17000000;
    am_ui_select(&model, &schedule, 16000000, &out); assert(out.page == AM_PAGE_INFO);
    am_ui_select(&model, &schedule, 17000000, &out); assert(out.page == AM_PAGE_TASK && out.current == 1);
    model.sessions[1].used = false;
    am_ui_select(&model, &schedule, 18000000, &out); assert(out.slots[out.current] == 0 && schedule.page_since_us == 18000000);
    strcpy(model.sessions[0].state, "working");
    am_ui_select(&model, &schedule, 19000000, &out); assert(out.group == AM_GROUP_RESULT);
    strcpy(model.alert_key, "s1"); strcpy(model.alert_provider, "codex"); assert(!am_event_in_group(&model));
    model.sessions[2].used = false;
    model.sessions[1] = session("s2", "stale", "claude");
    am_ui_select(&model, &schedule, 20000000, &out); assert(out.group == AM_GROUP_WORK && out.count == 2);
    am_ui_select(&model, &schedule, 30000000, &out); assert(out.current == 1);
    am_session_t task = session("x", "working", "codex");
    assert(am_task_duration(&task, 6000000) == 730);
    strcpy(task.state, "stale"); assert(am_task_duration(&task, 9000000) == 725);
    memset(model.sessions, 0, sizeof(model.sessions));
    model.usage_supported = true;
    model.usages[0] = (am_usage_t){.used = true, .used_bp = 4200, .window_seconds = 18000};
    strcpy(model.usages[0].provider, "codex"); strcpy(model.usages[0].metric, "b"); strcpy(model.usages[0].window, "primary");
    model.usages[1] = model.usages[0]; model.usages[1].window_seconds = 604800; strcpy(model.usages[1].metric, "a");
    am_ui_select(&model, &schedule, 31000000, &out);
    assert(out.page == AM_PAGE_USAGE && out.usage[0] == -1 && out.usage[1] == 1 && schedule.group == AM_GROUP_IDLE);
    am_ui_select(&model, &schedule, 40999999, &out); assert(out.page == AM_PAGE_USAGE);
    am_ui_select(&model, &schedule, 41000000, &out); assert(out.page == AM_PAGE_IDLE);
    am_ui_select(&model, &schedule, 51000000, &out); assert(out.page == AM_PAGE_USAGE);
    model.link = AM_LINK_SYNCING;
    am_ui_select(&model, &schedule, 52000000, &out); assert(out.page == AM_PAGE_CONNECT);
}

static void test_idle_rotation(void)
{
    am_app_t app = {0};
    app.model.link = AM_LINK_ONLINE;
    struct am_ui_schedule schedule = {0};
    struct am_ui_selection out;
    am_ui_select(&app.model, &schedule, 1000000, &out); assert(out.page == AM_PAGE_IDLE);
    am_ui_select(&app.model, &schedule, 20000000, &out); assert(out.page == AM_PAGE_IDLE);
    am_usage_t entry = {.used = true, .used_bp = -1, .window_seconds = 18000};
    strcpy(entry.provider, "claude"); strcpy(entry.metric, "l1"); strcpy(entry.window, "primary");
    assert(am_usage_write(&app, &entry) == AM_USAGE_OK && !app.model.usage_supported);
    strcpy(entry.provider, "codex"); entry.window_seconds = 0;
    assert(am_usage_write(&app, &entry) == AM_USAGE_OK && !app.model.usage_supported);
    entry.window_seconds = 18000;
    assert(am_usage_begin(&app, "codex", 1, 21000000) == AM_USAGE_OK);
    assert(am_usage_write(&app, &entry) == AM_USAGE_OK && !app.model.usage_supported);
    am_ui_select(&app.model, &schedule, 22000000, &out); assert(out.page == AM_PAGE_IDLE);
    app.usage_batch.failed = true;
    assert(am_usage_end(&app, "codex", 1) == AM_USAGE_BAD_STATE && !app.model.usage_supported);
    assert(am_usage_begin(&app, "codex", 2, 23000000) == AM_USAGE_OK);
    assert(am_usage_write(&app, &entry) == AM_USAGE_OK);
    am_usage_expire(&app, 53000000);
    assert(!app.model.usage_supported);
    assert(am_usage_begin(&app, "codex", 3, 54000000) == AM_USAGE_OK);
    assert(am_usage_write(&app, &entry) == AM_USAGE_OK);
    assert(am_usage_end(&app, "codex", 3) == AM_USAGE_OK && app.model.usage_supported);
    am_ui_select(&app.model, &schedule, 55000000, &out); assert(out.page == AM_PAGE_IDLE);
    for (int i = 1; i < 5; ++i) {
        entry.used_bp = i * 100;
        assert(am_usage_write(&app, &entry) == AM_USAGE_OK);
        am_ui_select(&app.model, &schedule, 55000000 + i * 1000000, &out);
        assert(out.page == AM_PAGE_USAGE && schedule.page_since_us == 56000000);
    }
    am_ui_select(&app.model, &schedule, 66000000, &out); assert(out.page == AM_PAGE_IDLE);
    am_usage_clear(&app, "codex"); assert(app.model.usage_supported);
    am_ui_select(&app.model, &schedule, 75000000, &out);
    assert(out.page == AM_PAGE_IDLE && out.usage[0] == -1 && out.usage[1] == -1);
    app.model.sessions[0] = session("s1", "permission_required", "claude");
    am_ui_select(&app.model, &schedule, 76000000, &out); assert(out.page == AM_PAGE_TASK);
    strcpy(app.model.sessions[0].state, "task_success");
    am_ui_select(&app.model, &schedule, 77000000, &out); assert(out.page == AM_PAGE_RESULT);
    strcpy(app.model.sessions[0].state, "ready");
    am_ui_select(&app.model, &schedule, 78000000, &out); assert(out.page == AM_PAGE_IDLE);
    am_ui_select(&app.model, &schedule, 88000000, &out); assert(out.page == AM_PAGE_IDLE);
    app.model.usage_supported = false; app.model.remote_generation++;
    am_ui_select(&app.model, &schedule, 89000000, &out); assert(out.page == AM_PAGE_IDLE);
    assert(am_usage_write(&app, &entry) == AM_USAGE_OK);
    am_ui_select(&app.model, &schedule, 90000000, &out); assert(out.page == AM_PAGE_USAGE);
}

static void test_standby_usage_availability(void)
{
    am_app_t app = {0};
    app.model.link = AM_LINK_ONLINE;
    struct am_ui_schedule schedule = {0};
    struct am_ui_selection selection;
    am_usage_t entry = {.used = true, .used_bp = -1, .window_seconds = 18000};
    strcpy(entry.provider, "codex");
    strcpy(entry.metric, "a");
    strcpy(entry.window, "primary");
    assert(am_usage_write(&app, &entry) == AM_USAGE_OK);
    entry.window_seconds = 604800;
    strcpy(entry.window, "secondary");
    assert(am_usage_write(&app, &entry) == AM_USAGE_OK);
    for (int i = 0; i < 4; ++i) {
        am_ui_select(&app.model, &schedule, i * AM_PAGE_DWELL_US, &selection);
        assert(app.model.usage_supported && selection.page == AM_PAGE_IDLE);
        assert(selection.usage[0] == -1 && selection.usage[1] == -1);
    }
    /* An unknown metric sorting first must not shadow a later metric with a valid reading. */
    strcpy(entry.metric, "z");
    entry.used_bp = 0;
    assert(am_usage_write(&app, &entry) == AM_USAGE_OK);
    am_ui_select(&app.model, &schedule, 40000000, &selection);
    assert(selection.page == AM_PAGE_USAGE && selection.usage[0] == -1 && selection.usage[1] >= 0);
    assert(strcmp(app.model.usages[selection.usage[1]].metric, "z") == 0);
    /* Losing the last usable reading must immediately cancel the usage page. */
    entry.used_bp = -1;
    assert(am_usage_write(&app, &entry) == AM_USAGE_OK);
    am_ui_select(&app.model, &schedule, 41000000, &selection);
    assert(selection.page == AM_PAGE_IDLE);
    /* A fully consumed window is still valid, and recovery starts a fresh ten-second dwell. */
    entry.used_bp = 10000;
    assert(am_usage_write(&app, &entry) == AM_USAGE_OK);
    am_ui_select(&app.model, &schedule, 42000000, &selection);
    assert(selection.page == AM_PAGE_USAGE && schedule.page_since_us == 42000000);
    am_ui_select(&app.model, &schedule, 51999999, &selection);
    assert(selection.page == AM_PAGE_USAGE);
    am_ui_select(&app.model, &schedule, 52000000, &selection);
    assert(selection.page == AM_PAGE_IDLE);
    am_usage_clear(&app, "codex");
    am_ui_select(&app.model, &schedule, 62000000, &selection);
    assert(selection.page == AM_PAGE_IDLE && app.model.usage_supported);
}

static void test_full_session_rotation(void)
{
    am_model_t model = {.link = AM_LINK_ONLINE};
    struct am_ui_schedule schedule = {0};
    struct am_ui_selection selection;
    assert(AM_MAX_SESSIONS == 8 && AM_MAX_USAGE_ENTRIES == 4);
    for (int i = 0; i < AM_MAX_SESSIONS; ++i) {
        char key[9];
        snprintf(key, sizeof(key), "s%d", i + 1);
        model.sessions[i] = session(key, "working", i % 2 ? "claude" : "codex");
    }
    for (int i = 0; i <= AM_MAX_SESSIONS; ++i) {
        am_ui_select(&model, &schedule, 1000000 + i * 10000000LL, &selection);
        assert(selection.page == AM_PAGE_TASK && selection.count == AM_MAX_SESSIONS);
        assert(selection.current == i % AM_MAX_SESSIONS);
    }
    model.sessions[0].used = false;
    am_ui_select(&model, &schedule, 82000000, &selection);
    assert(selection.count == AM_MAX_SESSIONS - 1 && selection.slots[selection.current] == 1);
}

static void test_calendar_and_led(void)
{
    am_model_t model = {.link = AM_LINK_ONLINE, .clock_valid = true, .utc_offset_valid = true, .utc_offset_minutes = -240};
    am_usage_t usage = {.reset_at = 1793518200, .window_seconds = 18000, .reset_offset_valid = true, .reset_utc_offset_minutes = -300};
    char value[64], date[32];
    /* 2026-11-01: reset offset differs from the current offset across DST. */
    reset_text(&model, &usage, 1793511000, date, sizeof(date), value, sizeof(value));
    assert(strcmp(value, "02:30") == 0);
    usage.reset_offset_valid = false;
    reset_text(&model, &usage, 1793511000, date, sizeof(date), value, sizeof(value));
    assert(strcmp(value, "未知") == 0);
    usage.reset_offset_valid = true;
    usage.reset_at = 1798780740;
    reset_text(&model, &usage, 1793511000, date, sizeof(date), value, sizeof(value));
    assert(strcmp(date, "2027/1/1") == 0 && strcmp(value, "00:19") == 0);
    assert(am_text_width(&g_am_font_label, "2027/12/31") <= 84);

}

static void check_safe_text(void)
{
    for (int i = 0; i < s_scene.count; ++i) {
        const struct am_ui_op *op = &s_scene.ops[i];
        if (op->kind != AM_OP_TEXT || op->y < 39) {
            continue;
        }
        const char *text = op->text;
        int cursor = op->x;
        while (*text) {
            uint32_t code = am_utf8_next(&text);
            const struct am_glyph *glyph = NULL;
            for (int j = 0; j < op->font->count; ++j) {
                if (op->font->glyphs[j].code == code) {
                    glyph = &op->font->glyphs[j];
                    break;
                }
            }
            assert(glyph);
            int numerator = op->font->scale_num, denominator = op->font->scale_den;
            int glyph_width = (glyph->width * numerator + denominator / 2) / denominator;
            int glyph_height = (glyph->height * numerator + denominator / 2) / denominator;
            int glyph_top = (glyph->top * numerator + denominator / 2) / denominator;
            for (int y = 0; y < glyph_height; ++y) {
                for (int x = 0; x < glyph_width; ++x) {
                    int sx = x * denominator / numerator, sy = y * denominator / numerator;
                    int bit = sy * glyph->width + sx;
                    uint8_t packed = op->font->pixels[glyph->offset + bit / 2];
                    int alpha = bit % 2 ? packed & 15 : packed >> 4;
                    int dx = (cursor + x) * 2 - 239;
                    int dy = (op->y + glyph_top + y) * 2 - 239;
                    /* The approved Session board uses a 230 px guide; other pages retain 216 px. */
                    int safe_diameter = s_scene.session_background ? 230 : 216;
                    if (alpha > 3 && dx * dx + dy * dy > safe_diameter * safe_diameter) {
                        fprintf(stderr, "Outside safe circle: %s at %d,%d\n", op->text, cursor + x, op->y + glyph_top + y);
                        assert(0);
                    }
                }
            }
            cursor += (glyph->advance * numerator + denominator / 2) / denominator;
        }
    }
}

static void write_ppm(const char *directory, const char *name)
{
    check_safe_text();
    char path[1024]; snprintf(path, sizeof(path), "%s/%s.ppm", directory, name);
    FILE *f = fopen(path,"wb"); assert(f); fprintf(f,"P6\n240 240\n255\n");
    for (int y = 0; y < 240; ++y) {
        for (int x = 0; x < 240; ++x) {
            uint16_t pixel = s_screen[y * 240 + x];
            int dx = x * 2 - 239, dy = y * 2 - 239;
            unsigned char rgb[3] = {(unsigned char)(((pixel>>11)&31)*255/31),(unsigned char)(((pixel>>5)&63)*255/63),
                                     (unsigned char)((pixel&31)*255/31)};
            if (dx * dx + dy * dy > 240 * 240) { memset(rgb, 255, sizeof(rgb)); }
            fwrite(rgb,1,3,f);
        }
    }
    fclose(f);
}

static void check_incremental(am_app_t *app, struct am_ui_scene *previous)
{
    assert(am_scene_render(app, previous, &s_scene, false) == ESP_OK);
    uint16_t actual[240*240]; memcpy(actual,s_screen,sizeof(actual));
    struct am_ui_scene full = {0};
    assert(am_scene_render(app,&full,&s_scene,true) == ESP_OK);
    assert(memcmp(actual,s_screen,sizeof(actual)) == 0);
}

static const struct am_ui_op *scene_text(const char *text)
{
    for (int i = 0; i < s_scene.count; ++i) {
        if (s_scene.ops[i].kind == AM_OP_TEXT && strcmp(s_scene.ops[i].text, text) == 0) {
            return &s_scene.ops[i];
        }
    }
    return NULL;
}

static void test_directory_fonts(void)
{
    am_model_t model = {0};
    am_session_t current = session("s1", "working", "codex");
    struct am_ui_selection selection = {.count = 1};
    const char *names[] = {"agentmate", "工程目录", "agentmate-provider-registry-dashboard", "", "proj-中🚀é·…", "café", "A B_42-~.()"};
    const char *expected[] = {"agentmate", "????", NULL, "", "proj-?????", "caf?", "A B_42-~.()"};
    assert(g_am_font_session_directory.count == 96);
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); ++i) {
        strcpy(current.project_name, names[i]);
        memset(&s_scene, 0, sizeof(s_scene));
        session_page(&model, &current, &selection, 1000000, 0, 1790936702);
        assert(s_scene.session_background && s_scene.count < AM_UI_OPS);
        const struct am_ui_op *project = NULL;
        for (int j = 0; j < s_scene.count; ++j) {
            const struct am_ui_op *op = &s_scene.ops[j];
            if (op->kind == AM_OP_TEXT && op->y == 48) {
                project = op;
            }
            if (op->kind == AM_OP_BITMAP) {
                assert(op->x == 17 && op->y == 117 && op->width == 38 && op->height == 38);
            }
        }
        if (i == 3) {
            assert(!project);
        } else {
            assert(project && project->font == &g_am_font_session_directory);
            assert(project->x >= 30 && project->x + project->width <= 210);
            assert(project->color == TFT_RGB565(101, 80, 200));
            assert(i == 2 ? strstr(project->text, "…") != NULL : strcmp(project->text, expected[i]) == 0);
        }
        assert(strcmp(current.project_name, names[i]) == 0);
    }
}

static void test_usage_colors(void)
{
    static const struct {
        int used_bp;
        const char *text;
        uint16_t color;
    } s_cases[] = {
        {0, "100.0%", C_MINT}, {5000, "50.0%", C_MINT}, {5001, "50.0%", C_MINT}, {5005, "50.0%", C_MINT},
        {5006, "49.9%", C_AMBER}, {8000, "20.0%", C_AMBER}, {8001, "20.0%", C_AMBER}, {8005, "20.0%", C_AMBER},
        {8006, "19.9%", C_CORAL}, {10000, "0.0%", C_CORAL}, {-1, "用量未知", C_MUTED},
    };
    am_app_t app = {0};
    am_model_t model = {0};
    model.usages[0] = (am_usage_t){.used = true, .window_seconds = 18000};
    model.usages[1] = (am_usage_t){.used = true, .window_seconds = 604800, .used_bp = 9200};
    struct am_ui_selection selection = {.usage = {0, 1}};
    struct am_ui_scene previous = {0};
    for (int i = 0; i < sizeof(s_cases) / sizeof(s_cases[0]); ++i) {
        model.usages[0].used_bp = s_cases[i].used_bp;
        memset(&s_scene, 0, sizeof(s_scene)); s_scene.body_alpha = 255; business_header(true);
        usage_page(&model, &selection, 0);
        int values = 0;
        for (int j = 0; j < s_scene.count; ++j) {
            const struct am_ui_op *op = &s_scene.ops[j];
            if (op->kind == AM_OP_TEXT && op->y >= 119 && op->y < 140) {
                assert(op->color == (values ? C_CORAL : s_cases[i].color));
                assert(strcmp(op->text, values ? "8.0%" : s_cases[i].text) == 0);
                values++;
            }
        }
        assert(values == 2);
        s_submitted_pixels = 0;
        assert(am_scene_render(&app, &previous, &s_scene, false) == ESP_OK);
        if (i) {
            assert(s_submitted_pixels < 5000);
            if (strcmp(s_cases[i].text, s_cases[i - 1].text) == 0) {
                assert(s_submitted_pixels == 0);
            }
        }
        check_incremental(&app, &previous);
    }
}


static void test_session_quota(const char *directory)
{
    static const struct { int used_bp; const char *text; uint16_t color; } s_cases[] = {
        {0, "100%", S_GREEN}, {4200, "58%", S_GREEN}, {5050, "50%", S_GREEN}, {5051, "49%", S_AMBER},
        {6500, "35%", S_AMBER}, {8050, "20%", S_AMBER}, {8051, "19%", S_CORAL}, {9950, "1%", S_CORAL},
        {9951, "0%", S_CORAL}, {10000, "0%", S_CORAL},
    };
    am_app_t app = {0};
    am_model_t *model = &app.model;
    model->link = AM_LINK_ONLINE;
    model->clock_valid = true;
    model->utc_offset_valid = true;
    model->utc_offset_minutes = 480;
    model->sessions[0] = session("s1", "working", "codex");
    model->usages[0] = (am_usage_t){.used = true, .used_bp = 4200, .window_seconds = 18000};
    strcpy(model->usages[0].provider, "codex");
    strcpy(model->usages[0].metric, "b");
    strcpy(model->usages[0].window, "primary");
    model->usages[1] = model->usages[0];
    model->usages[1].window_seconds = 604800;
    strcpy(model->usages[1].metric, "a");
    struct am_ui_schedule schedule = {0};
    struct am_ui_selection selection;
    am_ui_select(model, &schedule, 1000000, &selection);
    assert(selection.page == AM_PAGE_TASK && selection.usage[0] == -1 && selection.usage[1] == 1);
    assert(selection.detail == AM_DETAIL_DURATION);
    model->usages[2] = model->usages[0];
    strcpy(model->usages[2].metric, "a");
    model->usages[3] = model->usages[2];
    strcpy(model->usages[3].window, "aaa");
    am_ui_select(model, &schedule, 1000000, &selection);
    assert(selection.usage[0] == 3 && selection.usage[1] == 1);
    strcpy(model->usages[3].provider, "claude");
    am_ui_select(model, &schedule, 10999999, &selection);
    assert(selection.detail == AM_DETAIL_DURATION);
    int64_t page_since = schedule.page_since_us;
    struct am_ui_scene previous = {0};
    for (size_t i = 0; i < sizeof(s_cases) / sizeof(s_cases[0]); ++i) {
        model->usages[2].used_bp = s_cases[i].used_bp;
        model->usages[1].used_bp = 9200;
        am_ui_select(model, &schedule, 11000000, &selection);
        assert(selection.detail == AM_DETAIL_QUOTA && selection.usage[0] == 2 && selection.usage[1] == 1);
        assert(schedule.page_since_us == page_since);
        memset(&s_scene, 0, sizeof(s_scene)); s_scene.body_alpha = 255;
        session_page(model, &model->sessions[0], &selection, 11000000, 0, 1790936702);
        assert(s_scene.count < AM_UI_OPS && scene_text("5H") && scene_text("WEEK"));
        const struct am_ui_op *value = scene_text(s_cases[i].text);
        assert(value && value->color == s_cases[i].color && value->x >= 80 && value->x + value->width <= 141);
        assert(scene_text("8%") && scene_text("8%")->x > 149);
        int panels = 0;
        for (int j = 0; j < s_scene.count; ++j) {
            const struct am_ui_op *op = &s_scene.ops[j];
            if (op->kind == AM_OP_PANEL) {
                assert(op->x == 77 + panels * 72 && op->y == 148 && op->width == 67 && op->height == 42 && op->split_y == 15);
                panels++;
            }
        }
        assert(panels == 2);
        s_submitted_pixels = 0;
        assert(am_scene_render(&app, &previous, &s_scene, i == 0) == ESP_OK);
        if (i) {
            assert(s_submitted_pixels < 3500);
        }
        check_incremental(&app, &previous);
        if (i == 1) {
            write_ppm(directory, "session_quota_dual");
        }
    }
    /* Published atomic batches and each single window keep the panel at 67 px. */
    am_usage_t updated = model->usages[2]; updated.used_bp = 6500;
    assert(am_usage_begin(&app, "codex", 1, 1000000) == AM_USAGE_OK);
    assert(am_usage_write(&app, &updated) == AM_USAGE_OK);
    assert(model->usages[2].used_bp == 10000);
    assert(am_usage_end(&app, "codex", 1) == AM_USAGE_OK);
    for (int weekly = 0; weekly < 2; ++weekly) {
        if (weekly) {
            for (int i = 0; i < AM_MAX_USAGE_ENTRIES; ++i) {
                if (model->usages[i].used && strcmp(model->usages[i].provider, "codex") == 0) {
                    model->usages[i].window_seconds = 604800;
                }
            }
        }
        am_ui_select(model, &schedule, 11000000, &selection);
        assert(selection.usage[weekly] >= 0 && selection.usage[!weekly] == -1);
        memset(&s_scene, 0, sizeof(s_scene)); s_scene.body_alpha = 255;
        session_page(model, &model->sessions[0], &selection, 11000000, 0, 1790936702);
        assert(scene_text("35%") && scene_text(weekly ? "WEEK" : "5H") && !scene_text(weekly ? "5H" : "WEEK"));
        const struct am_ui_op *panel = &s_scene.ops[s_scene.count - 3];
        assert(panel->kind == AM_OP_PANEL && panel->x == 113 && panel->width == 67);
        check_incremental(&app, &previous);
        write_ppm(directory, weekly ? "session_quota_week" : "session_quota_5h");
    }
    model->sessions[0].context_mode = 2;
    model->sessions[0].ctx_pct_bp = 2500;
    model->sessions[0].ctx_used = 50000;
    model->sessions[0].ctx_window = 200000;
    model->revision++;
    am_ui_select(model, &schedule, 14000000, &selection);
    assert(selection.detail == AM_DETAIL_CONTEXT && schedule.page_since_us == page_since);
    static const struct { uint64_t used, total; const char *a, *b; } s_tokens[] = {
        {50000, 200000, "50K", "200K"}, {768000, 1234567, "768K", "1.23M"}, {7654300, 12345678, "7.65M", "12.3M"},
        {999499, 999500, "999K", "1.00M"}, {9994999, 9995000, "9.99M", "10.0M"},
        {99949999, 99950000, "99.9M", "100M"}, {0, 123456789, "0", "123M"}, {999, UINT64_MAX, "999", ">999M"},
    };
    for (size_t i = 0; i < sizeof(s_tokens) / sizeof(s_tokens[0]); ++i) {
        model->sessions[0].ctx_used = s_tokens[i].used;
        model->sessions[0].ctx_window = s_tokens[i].total;
        memset(&s_scene, 0, sizeof(s_scene)); s_scene.body_alpha = 255;
        session_page(model, &model->sessions[0], &selection, 14000000, 0, 1790936702);
        assert(scene_text("CTX") && scene_text("25%"));
        const struct am_ui_op *used = scene_text(s_tokens[i].a), *total = scene_text(s_tokens[i].b);
        if (!used || !total) {
            fprintf(stderr, "Token case %zu: expected %s / %s, got %s / %s\n", i, s_tokens[i].a, s_tokens[i].b,
                    s_scene.ops[s_scene.count - 2].text, s_scene.ops[s_scene.count - 1].text);
        }
        assert(used && total && used->y == 148 && total->y == 169);
        assert(used->x + am_text_width(used->font, used->text) == 210 && total->x + am_text_width(total->font, total->text) == 210);
        assert(used->width <= 54 && total->width <= 54);
        check_incremental(&app, &previous);
        if (i < 3) {
            write_ppm(directory, i == 0 ? "session_context_k" : i == 1 ? "session_context_m" : "session_context_large");
        }
    }
    const unsigned percentages[] = {0, 4900, 5000, 7900, 8000, 10000};
    for (size_t i = 0; i < sizeof(percentages) / sizeof(percentages[0]); ++i) {
        model->sessions[0].ctx_pct_bp = percentages[i];
        memset(&s_scene, 0, sizeof(s_scene)); s_scene.body_alpha = 255;
        session_context(&model->sessions[0]);
        assert(s_scene.ops[2].color == (i < 2 ? S_GREEN : i < 4 ? S_AMBER : S_CORAL));
    }
    model->sessions[0].context_mode = 1;
    memset(&s_scene, 0, sizeof(s_scene)); s_scene.body_alpha = 255;
    session_page(model, &model->sessions[0], &selection, 14000000, 0, 1790936702);
    assert(!scene_text("CTX"));
    check_incremental(&app, &previous); write_ppm(directory, "session_context_unknown");
    model->sessions[0].context_mode = 0;
    am_usage_clear(&app, "codex");
    am_ui_select(model, &schedule, 17000000, &selection);
    assert(selection.detail == AM_DETAIL_DURATION && selection.usage[0] == -1 && selection.usage[1] == -1);
    memset(&s_scene, 0, sizeof(s_scene)); s_scene.body_alpha = 255;
    session_page(model, &model->sessions[0], &selection, 17000000, 0, 1790936702);
    check_incremental(&app, &previous); write_ppm(directory, "session_all_metrics_invalid");
}

static void test_detail_timing(void)
{
    static const struct { bool quota, context; int64_t age; enum am_session_detail detail; } s_cases[] = {
        {true, true, 0, AM_DETAIL_DURATION}, {true, true, 9999999, AM_DETAIL_DURATION},
        {true, true, 10000000, AM_DETAIL_QUOTA}, {true, true, 12999999, AM_DETAIL_QUOTA},
        {true, true, 13000000, AM_DETAIL_CONTEXT}, {true, true, 15999999, AM_DETAIL_CONTEXT},
        {true, true, 16000000, AM_DETAIL_DURATION},
        {true, false, 9999999, AM_DETAIL_DURATION}, {true, false, 10000000, AM_DETAIL_QUOTA},
        {true, false, 12999999, AM_DETAIL_QUOTA}, {true, false, 13000000, AM_DETAIL_DURATION},
        {false, true, 9999999, AM_DETAIL_DURATION}, {false, true, 10000000, AM_DETAIL_CONTEXT},
        {false, true, 12999999, AM_DETAIL_CONTEXT}, {false, true, 13000000, AM_DETAIL_DURATION},
        {false, false, 10000000, AM_DETAIL_DURATION}, {false, false, 100000000, AM_DETAIL_DURATION},
    };
    for (size_t i = 0; i < sizeof(s_cases) / sizeof(s_cases[0]); ++i) {
        am_model_t model = {.link = AM_LINK_ONLINE};
        model.sessions[0] = session("s1", "working", "codex");
        model.sessions[0].context_mode = s_cases[i].context ? 2 : 1;
        model.sessions[0].ctx_window = 200000;
        model.usages[0] = (am_usage_t){.used = true, .window_seconds = 604800, .used_bp = s_cases[i].quota ? 0 : -1};
        strcpy(model.usages[0].provider, "codex"); strcpy(model.usages[0].metric, "a");
        struct am_ui_schedule schedule = {0};
        struct am_ui_selection selection;
        am_ui_select(&model, &schedule, 1000000, &selection);
        assert(selection.detail == AM_DETAIL_DURATION);
        model.revision++;
        am_ui_select(&model, &schedule, 1000000 + s_cases[i].age, &selection);
        assert(selection.detail == s_cases[i].detail && schedule.page_since_us == 1000000);
        assert(s_cases[i].quota ? selection.usage[1] == 0 : selection.usage[1] == -1);
    }
    am_model_t model = {.link = AM_LINK_ONLINE};
    model.sessions[0] = session("s1", "working", "codex");
    model.sessions[0].context_mode = 2;
    model.sessions[0].ctx_window = 0;
    assert(!am_session_context_available(&model.sessions[0]));
    model.sessions[0].ctx_window = 200000; model.sessions[0].ctx_pct_bp = 10001;
    assert(!am_session_context_available(&model.sessions[0]));
    model.sessions[0].ctx_pct_bp = 0;
    assert(am_session_context_available(&model.sessions[0]));
    model.usages[0] = (am_usage_t){.used = true, .window_seconds = 18000, .used_bp = -1};
    strcpy(model.usages[0].provider, "codex"); strcpy(model.usages[0].metric, "a");
    model.usages[1] = model.usages[0]; model.usages[1].window_seconds = 604800; model.usages[1].used_bp = 10000;
    strcpy(model.usages[1].metric, "b");
    struct am_ui_schedule schedule = {0};
    struct am_ui_selection selection;
    am_ui_select(&model, &schedule, 1000000, &selection);
    assert(selection.usage[0] == -1 && selection.usage[1] == 1);
    model.usages[1].used_bp = 10001;
    am_ui_select(&model, &schedule, 11000000, &selection);
    assert(selection.detail == AM_DETAIL_CONTEXT && selection.usage[0] == -1 && selection.usage[1] == -1);
    /* Standby skips the same unavailable windows as the session detail rotation. */
    memset(&model.sessions[0], 0, sizeof(model.sessions[0]));
    am_ui_select(&model, &schedule, 11000001, &selection);
    assert(selection.page == AM_PAGE_IDLE && selection.usage[0] == -1 && selection.usage[1] == -1);
}

static void test_detail_fairness(void)
{
    /* Ten-second pages must eventually show all three details for every session count. */
    for (int count = 1; count <= AM_MAX_SESSIONS; ++count) {
        am_model_t model = {.link = AM_LINK_ONLINE};
        model.usages[0] = (am_usage_t){.used = true, .window_seconds = 604800, .used_bp = 4200};
        strcpy(model.usages[0].provider, "codex");
        strcpy(model.usages[0].metric, "tokens");
        for (int i = 0; i < count; ++i) {
            char key[9]; snprintf(key, sizeof(key), "s%d", i);
            model.sessions[i] = session(key, "working", "codex");
            model.sessions[i].context_mode = 2;
            model.sessions[i].ctx_window = 200000;
        }
        struct am_ui_schedule schedule = {0};
        struct am_ui_selection selection;
        unsigned seen[AM_MAX_SESSIONS] = {0};
        for (int tick = 0; tick < count * 20; ++tick) {
            am_ui_select(&model, &schedule, 1000000 + tick * 1000000LL, &selection);
            assert(selection.current == (tick / 10) % count);
            seen[selection.slots[selection.current]] |= 1U << selection.detail;
        }
        for (int i = 0; i < count; ++i) {
            assert(seen[i] == 7);
        }
        /* A reused slot must not inherit the deleted session's phase. */
        int slot = selection.slots[selection.current];
        strcpy(model.sessions[slot].key, "new");
        am_ui_select(&model, &schedule, schedule.detail_updated_us + 1, &selection);
        if (selection.slots[selection.current] == slot) {
            assert(selection.detail == AM_DETAIL_DURATION);
        }
    }
}

static void test_status_animation(const char *directory)
{
    am_model_t model = {0};
    static const char *s_states[] = {"working", "thinking", "waiting_for_input", "permission_required",
                                    "stale", "task_success", "task_failed", "task_canceled"};
    static const unsigned s_periods[] = {24, 40, 20, 20, 30, 40, 24, 40};
    static const uint16_t s_colors[] = {TFT_RGB565(54, 226, 154), TFT_RGB565(139, 153, 255), TFT_RGB565(255, 176, 46),
        TFT_RGB565(255, 95, 122), TFT_RGB565(163, 175, 211), TFT_RGB565(54, 226, 154), TFT_RGB565(255, 59, 48), TFT_RGB565(74, 168, 255)};
    am_app_t app = {0};
    struct am_ui_selection selection = {.count = 1};
    struct am_status_animation clock = {0};
    am_session_t current = session("motion", "working", "codex");
    struct am_status_visual visual = am_status_select(&clock, &current, 1, 1000000);
    assert(visual.frame == 0);
    visual = am_status_select(&clock, &current, 1, 1099999); assert(visual.frame == 0);
    visual = am_status_select(&clock, &current, 1, 1100000); assert(visual.frame == 1);
    current.rev++; current.task_seconds++; strcpy(current.project_name, "new_directory");
    visual = am_status_select(&clock, &current, 1, 1200000); assert(visual.frame == 2 && clock.started_us == 1000000);
    strcpy(current.key, "other");
    visual = am_status_select(&clock, &current, 1, 1300000); assert(visual.frame == 0);
    visual = am_status_select(&clock, &current, 2, 1400000); assert(visual.frame == 0 && clock.started_us == 1400000);
    strcpy(current.state, "stale");
    visual = am_status_select(&clock, &current, 2, 1500000); assert(visual.frame == 0 && visual.kind == AM_STATUS_STALE);
    current = session("s1", "working", "codex");
    char frames_directory[1024]; snprintf(frames_directory, sizeof(frames_directory), "%s/animation", directory);
    uint16_t first[240 * 240], previous_pixels[240 * 240];
    for (unsigned i = 0; i < 8; ++i) {
        strcpy(current.state, s_states[i]);
        struct am_ui_scene previous = {0};
        memset(&s_status_animation, 0, sizeof(s_status_animation));
        for (unsigned frame = 0; frame <= s_periods[i]; ++frame) {
            memset(&s_scene, 0, sizeof(s_scene)); s_scene.body_alpha = 255;
            session_page(&model, &current, &selection, 1000000 + frame * AM_STATUS_TICK_US, i + 10, 1790936702);
            const struct am_ui_op *border = NULL;
            const struct am_ui_op *label = NULL;
            for (int op = 0; op < s_scene.count; ++op) {
                if (s_scene.ops[op].kind == AM_OP_STATUS_FRAME) {
                    border = &s_scene.ops[op]; label = &s_scene.ops[op + 1];
                }
            }
            assert(border && border->animation_frame == frame % s_periods[i] && border->color == s_colors[i]);
            assert(label->kind == AM_OP_TEXT && label->color == s_colors[i] && strstr(label->text, "…") == NULL);
            assert(label->x >= 84 && label->x + label->width <= 215);
            s_submitted_pixels = 0;
            assert(am_scene_render(&app, &previous, &s_scene, frame == 0) == ESP_OK);
            assert(s_screen[114 * 240 + 100] == TFT_COLOR_BLACK);
            if (frame) {
                assert(s_submitted_pixels >= 145 * 56);
                assert(s_submitted_pixels <= 145 * 56 + 5000);
                bool changed = false;
                for (int y = 86; y < 142; ++y) {
                    for (int x = 77; x < 222; ++x) {
                        changed |= s_screen[y * 240 + x] != previous_pixels[y * 240 + x];
                    }
                }
                assert(changed);
                for (int y = 100; y < 128; ++y) {
                    assert(memcmp(first + y * 240 + 90, s_screen + y * 240 + 90, 117 * sizeof(uint16_t)) == 0);
                }
            } else {
                memcpy(first, s_screen, sizeof(first));
            }
            memcpy(previous_pixels, s_screen, sizeof(previous_pixels));
            check_incremental(&app, &previous);
            if (frame < s_periods[i]) {
                char name[96]; snprintf(name, sizeof(name), "%s_%02u", s_states[i], frame);
                write_ppm(frames_directory, name);
            }
        }
        /* Status borders repeat independently of the selected dot's breathing period. */
        if (!am_state_advancing(current.state)) {
            for (int y = 86; y < 142; ++y) {
                assert(memcmp(first + y * 240 + 77, s_screen + y * 240 + 77, 145 * sizeof(uint16_t)) == 0);
            }
        }
        s_submitted_pixels = 0;
        assert(am_scene_render(&app, &previous, &s_scene, false) == ESP_OK && s_submitted_pixels == 0);
    }
}

static void test_session_clock(const char *directory)
{
    am_app_t app = {0};
    am_model_t model = {.clock_valid = true, .utc_offset_valid = true, .utc_offset_minutes = 480};
    am_session_t current = session("s1", "task_success", "codex");
    struct am_ui_selection selection = {.count = 1, .usage = {-1, -1}};
    struct am_ui_scene previous = {0};
    uint16_t clock_on[240 * 76];
    const int64_t times[] = {0, 499999, 500000, 999999, 1000000};
    for (size_t i = 0; i < sizeof(times) / sizeof(times[0]); ++i) {
        memset(&s_scene, 0, sizeof(s_scene)); s_scene.body_alpha = 255;
        session_page(&model, &current, &selection, times[i], 0, 1790936702);
        assert(scene_text("12:05") && scene_text("12:05")->color == TFT_RGB565(141, 141, 141));
        assert(s_scene.ops[2].kind == AM_OP_TEXT && strcmp(s_scene.ops[2].text, ":") == 0);
        assert(s_scene.ops[1].font == &g_am_font_session_clock_rounded);
        assert(s_scene.ops[2].color == (i == 2 || i == 3 ? S_CAP : TFT_COLOR_WHITE));
        check_incremental(&app, &previous);
        if (i == 0) {
            memcpy(clock_on, s_screen, sizeof(clock_on));
            write_ppm(directory, "session_clock_on");
        } else if (i == 2) {
            int changes = 0;
            for (int y = 0; y < 76; ++y) {
                for (int x = 0; x < 240; ++x) {
                    if (clock_on[y * 240 + x] != s_screen[y * 240 + x]) {
                        const struct am_ui_op *colon = &s_scene.ops[2];
                        assert(x >= colon->x && x < colon->x + colon->width && y >= colon->y && y < colon->y + colon->height);
                        changes++;
                    }
                }
            }
            assert(changes > 0);
            write_ppm(directory, "session_clock_off");
        } else if (i == 1 || i == 4) {
            assert(memcmp(clock_on, s_screen, sizeof(clock_on)) == 0);
        }
    }
    /* A blinking separator during a body fade must not leave the header stale. */
    memset(&s_scene, 0, sizeof(s_scene)); s_scene.body_alpha = 128;
    session_page(&model, &current, &selection, 1500000, 0, 1790936702);
    check_incremental(&app, &previous);
    memset(&s_scene, 0, sizeof(s_scene)); s_scene.body_alpha = 255;
    session_page(&model, &current, &selection, 2000000, 0, 1790936760);
    check_incremental(&app, &previous);
    for (int invalid = 0; invalid < 2; ++invalid) {
        model.clock_valid = invalid != 0; model.utc_offset_valid = invalid == 0;
        memset(&s_scene, 0, sizeof(s_scene)); s_scene.body_alpha = 255;
        session_page(&model, &current, &selection, 2500000, 0, 1790936760);
        assert(strcmp(s_scene.ops[1].text, "--") == 0 && strcmp(s_scene.ops[3].text, "--") == 0);
        assert(s_scene.ops[2].color == TFT_COLOR_WHITE);
        check_incremental(&app, &previous);
    }
    /* Four-digit and maximal protocol durations stay complete, centered and frozen. */
    const uint32_t durations[] = {999 * 60 + 59, 1000 * 60, UINT32_MAX};
    for (size_t i = 0; i < sizeof(durations) / sizeof(durations[0]); ++i) {
        current.task_seconds = durations[i];
        memset(&s_scene, 0, sizeof(s_scene)); s_scene.body_alpha = 255;
        session_page(&model, &current, &selection, 3000000, 0, 1790936760);
        const struct am_ui_op *duration = &s_scene.ops[s_scene.count - 1];
        assert(duration->kind == AM_OP_TEXT && !strstr(duration->text, "…"));
        assert(duration->x >= 77 && duration->x + duration->width <= 222);
        assert(i == 0 ? duration->height == 32 : i == 1 ? duration->height == 26 : duration->height < 26);
        check_incremental(&app, &previous);
        write_ppm(directory, i == 0 ? "session_duration_999" : i == 1 ? "session_duration_1000" : "session_duration_max");
    }
}

static void test_idle_clock(const char *directory)
{
    am_app_t app = {0};
    am_model_t model = {.link = AM_LINK_ONLINE, .clock_valid = true, .utc_offset_valid = true, .utc_offset_minutes = 480};
    struct am_ui_scene previous = {0};
    static uint16_t s_visible[240 * 240];
    const int64_t times[] = {0, 499999, 500000, 999999, 1000000};
    for (int frame = 0; frame < 5; ++frame) {
        memset(&s_scene, 0, sizeof(s_scene));
        s_scene.body_alpha = 255;
        business_header(true);
        idle_page(&model, 1790936702, times[frame]);
        s_submitted_pixels = 0;
        assert(am_scene_render(&app, &previous, &s_scene, frame == 0) == ESP_OK);
        if (frame == 0) {
            memcpy(s_visible, s_screen, sizeof(s_visible));
            write_ppm(directory, "idle_colon_on");
        } else if (frame == 1 || frame == 3) {
            assert(s_submitted_pixels == 0);
        } else {
            assert(s_submitted_pixels == am_text_ink_width(&g_am_font_standby_clock, ":") * g_am_font_standby_clock.height);
        }
        if (frame == 2) {
            int changed = 0, left = 240, right = 0, top = 240, bottom = 0;
            for (int y = 0; y < 240; ++y) {
                for (int x = 0; x < 240; ++x) {
                    if (s_visible[y * 240 + x] != s_screen[y * 240 + x]) {
                        changed++;
                        left = x < left ? x : left;
                        right = x > right ? x : right;
                        top = y < top ? y : top;
                        bottom = y > bottom ? y : bottom;
                    }
                }
            }
            assert(changed > 0 && changed < 200 && right - left < 12);
            /* The square glyphs align the colon with the digits without a vertical offset. */
            assert(abs(top + bottom - 228) <= 2);
            write_ppm(directory, "idle_colon_off");
        }
        if (frame == 4) {
            assert(memcmp(s_visible, s_screen, sizeof(s_visible)) == 0);
        }
        check_incremental(&app, &previous);
    }
    /* Minute changes and valid/unknown transitions must match complete redraws. */
    memset(&s_scene, 0, sizeof(s_scene)); s_scene.body_alpha = 255; business_header(true);
    idle_page(&model, 1790936759, 1500000); check_incremental(&app, &previous);
    memset(&s_scene, 0, sizeof(s_scene)); s_scene.body_alpha = 255; business_header(true);
    idle_page(&model, 1790936760, 2000000); check_incremental(&app, &previous);
    for (int invalid = 0; invalid < 2; ++invalid) {
        model.clock_valid = invalid != 0;
        model.utc_offset_valid = invalid == 0;
        memset(&s_scene, 0, sizeof(s_scene)); s_scene.body_alpha = 255; business_header(true);
        idle_page(&model, 1790936702, 0); check_incremental(&app, &previous);
        memcpy(s_visible, s_screen, sizeof(s_visible));
        memset(&s_scene, 0, sizeof(s_scene)); s_scene.body_alpha = 255; business_header(true);
        idle_page(&model, 1790936702, 500000);
        s_submitted_pixels = 0;
        assert(am_scene_render(&app, &previous, &s_scene, false) == ESP_OK && s_submitted_pixels == 0);
        assert(memcmp(s_visible, s_screen, sizeof(s_visible)) == 0);
    }
    model.clock_valid = true;
    model.utc_offset_valid = true;
    memset(&s_scene, 0, sizeof(s_scene)); s_scene.body_alpha = 255; business_header(true);
    idle_page(&model, 1790936702, 700000); check_incremental(&app, &previous);
}

static void test_render(const char *directory)
{
    am_app_t app = {0}; strcpy(app.device_id,"0123456789abcdef01234567dc194fa2");
    am_model_t model = {.link=AM_LINK_ONLINE,.clock_valid=true,.utc_offset_valid=true,.utc_offset_minutes=480};
    struct am_ui_scene previous = {0};
    struct am_ui_selection sel = {.count=2};
    am_session_t s = session("s1","working","claude"); s.context_mode=2; s.ctx_used=12000; s.ctx_window=200000; s.ctx_pct_bp=600;
    s_now = 1000000;
    const char *states[] = {"working","thinking","waiting_for_input","permission_required","stale","task_success","task_failed","task_canceled"};
    for (int i=0;i<8;++i) {
        memset(&s_scene,0,sizeof(s_scene)); s_scene.body_alpha=255;
        strcpy(s.state,states[i]); session_page(&model, &s,&sel,s_now, 0, 1790936702); check_incremental(&app,&previous); write_ppm(directory,states[i]);
    }
    strcpy(s.host_label, "WWWWWWWW");
    strcpy(s.session_short_name, "S4294967295");
    memset(&s_scene, 0, sizeof(s_scene)); s_scene.body_alpha = 255; session_page(&model, &s, &sel, s_now, 0, 1790936702);
    bool identity_found = false;
    for (int i = 0; i < s_scene.count; ++i) {
        if (s_scene.ops[i].kind == AM_OP_TEXT && s_scene.ops[i].y == 209 && strstr(s_scene.ops[i].text, "…") != NULL) {
            identity_found = true;
            assert(s_scene.ops[i].x >= 84 && s_scene.ops[i].x + s_scene.ops[i].width <= 156);
        }
    }
    assert(identity_found);
    check_incremental(&app, &previous); write_ppm(directory, "long_host_title");
    strcpy(s.host_label, "WSL"); strcpy(s.session_short_name, "S1");
    sel.count = AM_MAX_SESSIONS; sel.current = AM_MAX_SESSIONS - 1;
    memset(&s_scene, 0, sizeof(s_scene)); s_scene.body_alpha = 255; session_page(&model, &s, &sel, s_now, 0, 1790936702);
    check_incremental(&app, &previous); write_ppm(directory, "eight_sessions");
    sel.count = 2; sel.current = 0;
    const char *projects[] = {"agentmate", "工程目录", "agentmate-provider-registry-dashboard"};
    const char *fixtures[] = {"codex_short_directory", "codex_chinese_directory", "codex_long_directory"};
    am_session_t codex = session("s1", "working", "codex");
    for (int i = 0; i < 3; ++i) {
        strcpy(codex.project_name, projects[i]);
        memset(&s_scene, 0, sizeof(s_scene)); s_scene.body_alpha = 255; session_page(&model, &codex, &sel, s_now, 0, 1790936702);
        check_incremental(&app, &previous); write_ppm(directory, fixtures[i]);
    }
    strcpy(s.state,"working");
    memset(&s_scene,0,sizeof(s_scene)); s_scene.body_alpha=255; session_page(&model, &s,&sel,s_now, 0, 1790936702);
    check_incremental(&app,&previous); s_submitted_pixels=0;
    memset(&s_scene,0,sizeof(s_scene)); s_scene.body_alpha=255; session_page(&model, &s,&sel,s_now+1000000, 0, 1790936702);
    am_scene_render(&app,&previous,&s_scene,false); assert(s_submitted_pixels <= 145 * 56 + 5000);
    s.context_mode=1; strcpy(s.project_name,"工程目录-agentmate-provider-registry-dashboard"); strcpy(s.key,"ABCDEFGH"); s.task_seconds=UINT32_MAX;
    memset(&s_scene,0,sizeof(s_scene)); s_scene.body_alpha=255; session_page(&model, &s,&sel,s_now, 0, 1790936702);
    check_incremental(&app,&previous); write_ppm(directory,"long_session");
    memset(&s_scene,0,sizeof(s_scene)); s_scene.body_alpha=255; business_header(true); info_page(&app,&model,230,102,1790936702);
    check_incremental(&app,&previous); write_ppm(directory,"info");
    memset(&s_scene,0,sizeof(s_scene)); s_scene.body_alpha=255; business_header(true); idle_page(&model,1790936702, 0);
    check_incremental(&app,&previous); write_ppm(directory,"idle");
    model.utc_offset_valid=false;
    memset(&s_scene,0,sizeof(s_scene)); s_scene.body_alpha=255; business_header(true); info_page(&app,&model,230,102,1790936702);
    check_incremental(&app,&previous); write_ppm(directory,"info_unknown");
    memset(&s_scene,0,sizeof(s_scene)); s_scene.body_alpha=255; business_header(true); idle_page(&model,1790936702, 0);
    check_incremental(&app,&previous); write_ppm(directory,"idle_unknown");
    const am_link_t links[]={AM_LINK_WAIT_HOST,AM_LINK_HANDSHAKE,AM_LINK_SYNCING};
    const char *link_names[]={"wait_host","handshake","syncing"};
    for(int i=0;i<3;++i) {
        model.link=links[i]; memset(&s_scene,0,sizeof(s_scene)); s_scene.body_alpha=255; business_header(false); connection_page(&model);
        check_incremental(&app,&previous); write_ppm(directory,link_names[i]);
    }
    model.utc_offset_valid=true; model.usages[0]=(am_usage_t){.used=true,.used_bp=4200,.window_seconds=18000,.reset_at=1790944200,
                                                        .reset_offset_valid=true,.reset_utc_offset_minutes=480};
    model.usages[1]=model.usages[0]; model.usages[1].window_seconds=604800; model.usages[1].used_bp=1850; model.usages[1].reset_at=1791540000;
    sel.usage[0]=0;sel.usage[1]=1;
    memset(&s_scene,0,sizeof(s_scene)); s_scene.body_alpha=255; business_header(true); usage_page(&model,&sel,1790936702);
    check_incremental(&app,&previous); write_ppm(directory,"usage");
    model.usages[0].used_bp = 6500; model.usages[1].used_bp = 9200;
    memset(&s_scene, 0, sizeof(s_scene)); s_scene.body_alpha = 255; business_header(true); usage_page(&model, &sel, 1790936702);
    check_incremental(&app, &previous); write_ppm(directory, "usage_attention_low");
    model.usages[0].used_bp = 5000; model.usages[1].used_bp = 8000;
    memset(&s_scene, 0, sizeof(s_scene)); s_scene.body_alpha = 255; business_header(true); usage_page(&model, &sel, 1790936702);
    check_incremental(&app, &previous); write_ppm(directory, "usage_thresholds");
    model.usages[0].used_bp = 4200; model.usages[1].used_bp = 1850;
    memset(&s_scene, 0, sizeof(s_scene)); s_scene.body_alpha = 255; business_header(true); usage_page(&model, &sel, 1790936702);
    check_incremental(&app, &previous);
    s_submitted_pixels = 0;
    assert(am_scene_render(&app, &previous, &s_scene, false) == ESP_OK && s_submitted_pixels == 0);
    model.usages[0].used_bp = 4300;
    memset(&s_scene, 0, sizeof(s_scene)); s_scene.body_alpha = 255; business_header(true); usage_page(&model, &sel, 1790936702);
    assert(am_scene_render(&app, &previous, &s_scene, false) == ESP_OK && s_submitted_pixels < 5000);
    check_incremental(&app, &previous);
    for (int i = 0; i < s_scene.count; ++i) {
        const struct am_ui_op *op = &s_scene.ops[i];
        assert(op->kind != AM_OP_TEXT || strcmp(op->text, "重置时间") != 0);
    }
    model.usages[0].used_bp=-1;
    memset(&s_scene,0,sizeof(s_scene)); s_scene.body_alpha=255; business_header(true); usage_page(&model,&sel,1790936702);
    check_incremental(&app,&previous); write_ppm(directory,"usage_unknown");
    sel.usage[1]=-1;
    memset(&s_scene,0,sizeof(s_scene)); s_scene.body_alpha=255; business_header(true); usage_page(&model,&sel,1790936702);
    check_incremental(&app,&previous); write_ppm(directory,"usage_single");
    sel.usage[0] = -1; sel.usage[1] = 1;
    model.usages[1].reset_at = 1830297540;
    memset(&s_scene,0,sizeof(s_scene)); s_scene.body_alpha=255; business_header(true); usage_page(&model,&sel,1790936702);
    check_incremental(&app,&previous); write_ppm(directory,"usage_week_single");
    sel.usage[0] = 0;
    memset(&s_scene,0,sizeof(s_scene)); s_scene.body_alpha=255; business_header(true); usage_page(&model,&sel,1790936702);
    check_incremental(&app,&previous); write_ppm(directory,"usage_cross_year");
    model.usages[1].reset_offset_valid = false;
    memset(&s_scene,0,sizeof(s_scene)); s_scene.body_alpha=255; business_header(true); usage_page(&model,&sel,1790936702);
    check_incremental(&app,&previous); write_ppm(directory,"usage_reset_unknown");
    sel.usage[0] = -1; sel.usage[1] = -1;
    memset(&s_scene, 0, sizeof(s_scene)); s_scene.body_alpha = 255; business_header(true); usage_page(&model, &sel, 1790936702);
    check_incremental(&app, &previous); write_ppm(directory, "usage_cleared");
    int unknown_count = 0, placeholder_count = 0;
    for (int i = 0; i < s_scene.count; ++i) {
        if (s_scene.ops[i].kind == AM_OP_TEXT) {
            unknown_count += strcmp(s_scene.ops[i].text, "用量未知") == 0;
            placeholder_count += strcmp(s_scene.ops[i].text, "--") == 0;
        }
    }
    assert(unknown_count == 2 && placeholder_count == 3);
    memset(&s_scene,0,sizeof(s_scene)); s_scene.body_alpha=255; boot_page();
    check_incremental(&app,&previous); write_ppm(directory,"boot");
    for (int i=0;i<5;++i) { assert(render_font_sample(&app,i,0)==ESP_OK); }
    struct am_led_effect effect = {0};
    struct am_led_state led_state = {0};
    rgb_led_color_t colors[16];
    assert(animate_led(&app, &led_state, &effect, 1000000, colors, true) == ESP_OK);
    for (int i = 0; i < 16; ++i) {
        assert(s_ring[i].r == 0 && s_ring[i].g == 0 && s_ring[i].b == 0);
    }
}

static void test_clock_themes(const char *directory)
{
    am_app_t app = {0};
    am_model_t model = {.link = AM_LINK_ONLINE, .clock_valid = true, .utc_offset_valid = true, .utc_offset_minutes = 480};
    model.clock_rgb = 0x12abef;
    struct am_ui_scene previous = {0};
    const uint16_t colors[] = {TFT_RGB565(241, 188, 116), TFT_RGB565(246, 245, 255),
                               TFT_RGB565(119, 230, 188), TFT_RGB565(182, 160, 242), TFT_RGB565(18, 171, 239)};
    const uint16_t dates[] = {TFT_RGB565(211, 223, 221), TFT_RGB565(226, 232, 246),
                              TFT_RGB565(206, 235, 221), TFT_RGB565(211, 200, 234), TFT_RGB565(100, 200, 244)};
    const char *names[] = {"idle_amber", "idle_white", "idle_mint", "idle_codex", "idle_custom"};
    for (unsigned theme = 0; theme <= AM_CLOCK_THEME_COUNT; ++theme) {
        /* Also exercise corrupted/future IDs falling back to the default palette. */
        model.clock_theme = theme < AM_CLOCK_THEME_COUNT ? theme : UINT8_MAX;
        unsigned expected = theme < AM_CLOCK_THEME_COUNT ? theme : AM_CLOCK_AMBER;
        for (int invalid = 0; invalid < 2; ++invalid) {
            model.clock_valid = !invalid;
            for (int phase = 0; phase < 2; ++phase) {
                memset(&s_scene, 0, sizeof(s_scene)); s_scene.body_alpha = 255; business_header(true);
                idle_page(&model, 1790936702, phase * 500000);
                check_incremental(&app, &previous);
                check_safe_text();
                const struct am_ui_op *day = scene_text(invalid ? "-- --" : "10-02");
                assert(day && day->color == (invalid ? TFT_RGB565(141, 152, 183) : dates[expected]));
                for (int i = 0; i < s_scene.count; ++i) {
                    const struct am_ui_op *op = &s_scene.ops[i];
                    if (op->kind == AM_OP_TEXT && op->font == &g_am_font_standby_clock && strcmp(op->text, ":") != 0) {
                        assert(op->color == (invalid ? TFT_RGB565(141, 152, 183) : colors[expected]));
                    }
                }
                assert(s_screen[38 * 240 + 120] == TFT_RGB565(31, 191, 116));
                if (!invalid && phase == 0 && theme < AM_CLOCK_THEME_COUNT) {
                    write_ppm(directory, names[theme]);
                }
            }
        }
    }
}

static void test_v62_pages(const char *directory)
{
    am_app_t app = {.tft_brightness = 192, .led_brightness = 64};
    am_model_t model = {.link = AM_LINK_ONLINE, .clock_valid = true, .utc_offset_valid = true, .utc_offset_minutes = 480};
    struct am_ui_scene previous = {0};
    for (int connected = 0; connected < 2; ++connected) {
        memset(&s_scene, 0, sizeof(s_scene));
        s_scene.body_alpha = 255;
        business_header(connected);
        idle_page(&model, 1790936702, 0);
        check_incremental(&app, &previous);
        uint16_t color = connected ? TFT_RGB565(31, 191, 116) : TFT_RGB565(232, 68, 92);
        assert(s_screen[38 * 240 + 120] == color && s_screen[39 * 240 + 120] == TFT_COLOR_BLACK);
        assert(s_screen[35 * 240 + 32] == TFT_COLOR_BLACK && s_screen[30 * 240 + 50] == color);
        assert(s_screen[29 * 240 + 32] != color && s_screen[29 * 240 + 32] != TFT_COLOR_BLACK);
        assert(scene_text("2026") && scene_text("10-02"));
    }
    /* Curved headers must still match full redraws when the body fades or the link changes. */
    for (int phase = 0; phase < 3; ++phase) {
        memset(&s_scene, 0, sizeof(s_scene));
        s_scene.body_alpha = phase == 0 ? 0 : phase == 1 ? 128 : 255;
        business_header(phase != 1);
        connection_page(&model);
        check_incremental(&app, &previous);
    }
    /* Exercise day/year rollover and the widest day without clipping the enlarged date. */
    const int64_t dates[] = {1798732799, 1798732800, 1803744000};
    for (int i = 0; i < 3; ++i) {
        memset(&s_scene, 0, sizeof(s_scene)); s_scene.body_alpha = 255; business_header(true);
        idle_page(&model, dates[i], 0);
        check_incremental(&app, &previous);
        check_safe_text();
    }
    model.usages[0] = (am_usage_t){.used = true, .used_bp = 0, .window_seconds = 18000};
    model.usages[1] = (am_usage_t){.used = true, .used_bp = 10000, .window_seconds = 604800};
    struct am_ui_selection selection = {.usage = {0, 1}};
    memset(&s_scene, 0, sizeof(s_scene)); s_scene.body_alpha = 255; business_header(true);
    usage_page(&model, &selection, 1790936702); check_incremental(&app, &previous);
    assert(scene_text("100.0%") && scene_text("0.0%"));
    write_ppm(directory, "usage_extremes");
    selection.usage[0] = -1;
    memset(&s_scene, 0, sizeof(s_scene)); s_scene.body_alpha = 255; business_header(true);
    usage_page(&model, &selection, 1790936702); check_incremental(&app, &previous);
    assert(s_screen[110 * 240 + 35] == TFT_COLOR_BLACK);
    assert(s_screen[110 * 240 + 80] == S_QUOTA_TOP);
    model.test = AM_TEST_PAGE;
    assert(render_factory(&app, &model, 0) == ESP_OK);
    assert(scene_text("产测模式")); write_ppm(directory, "factory_home");
    model.test = AM_TEST_LED; model.test_led_index = 16;
    model.test_led_color = RGB_LED_COLOR_RED; model.test_led_brightness = 96;
    assert(render_factory(&app, &model, 0) == ESP_OK);
    assert(scene_text("LED 全灯")); write_ppm(directory, "factory_led_all");
    model.test_led_index = 4;
    assert(render_factory(&app, &model, 0) == ESP_OK);
    assert(scene_text("LED 05 / 16")); write_ppm(directory, "factory_led_single");
    for (int i = 0; i < 16; ++i) {
        assert(s_ring[i].r == (i == 4 ? 255 : 0));
    }
    model.test = AM_TEST_BACKLIGHT; model.test_backlight = 128;
    assert(render_factory(&app, &model, 0) == ESP_OK);
    assert(scene_text("背光 128")); write_ppm(directory, "factory_backlight");
    for (int i = 0; i < 5; ++i) {
        assert(render_font_sample(&app, i, 0) == ESP_OK);
        char name[32]; snprintf(name, sizeof(name), "factory_font_%d", i);
        write_ppm(directory, name);
        for (int page = 1; page < 16; ++page) {
            assert(render_font_sample(&app, i, page) == ESP_OK);
        }
    }
    model.test = AM_TEST_AUTO; model.test_started_us = 0;
    for (int step = 0; step < 34; ++step) {
        assert(render_factory(&app, &model, step * 2000000LL) == ESP_OK);
        if (step == 2) {
            for (int pixel = 0; pixel < 240 * 240; ++pixel) {
                assert(s_screen[pixel] == TFT_COLOR_RED);
            }
        }
    }
}

static void test_usage_batches(void)
{
    am_app_t app = {0};
    am_usage_t old = {.used = true, .used_bp = 4200, .window_seconds = 18000};
    strcpy(old.provider, "codex");
    strcpy(old.metric, "l1");
    strcpy(old.window, "primary");
    assert(am_usage_write(&app, &old) == AM_USAGE_OK);
    app.model.link = AM_LINK_ONLINE;
    assert(app.model.usage_supported);
    struct am_ui_schedule schedule = {0};
    struct am_ui_selection selection;
    uint32_t revision = app.model.revision;
    assert(am_usage_begin(&app, "codex", 1, 100) == AM_USAGE_OK);
    assert(am_usage_begin(&app, "codex", 1, 100) == AM_USAGE_OK);
    am_usage_t updated = old;
    updated.used_bp = 5100;
    assert(am_usage_write(&app, &updated) == AM_USAGE_OK);
    assert(app.model.usages[0].used_bp == 4200 && app.model.revision == revision);
    am_ui_select(&app.model, &schedule, 200, &selection);
    assert(selection.page == AM_PAGE_USAGE);
    assert(am_usage_end(&app, "codex", 2) == AM_USAGE_BAD_STATE);
    assert(am_usage_end(&app, "codex", 1) == AM_USAGE_OK);
    assert(app.model.usages[0].used_bp == 5100 && app.model.revision == revision + 1);
    assert(am_usage_end(&app, "codex", 1) == AM_USAGE_OK && app.model.revision == revision + 1);
    assert(am_usage_begin(&app, "codex", 1, 100) == AM_USAGE_BAD_STATE);
    am_usage_t other = old;
    strcpy(other.provider, "claude");
    assert(am_usage_write(&app, &other) == AM_USAGE_OK);
    assert(am_usage_begin(&app, "codex", 2, 100) == AM_USAGE_OK);
    assert(am_usage_end(&app, "codex", 2) == AM_USAGE_OK && app.model.usage_count == 1);
    assert(strcmp(app.model.usages[1].provider, "claude") == 0);
    assert(am_usage_begin(&app, "codex", 3, 100) == AM_USAGE_OK);
    for (int i = 0; i < AM_MAX_USAGE_ENTRIES - 1; ++i) {
        snprintf(updated.window, sizeof(updated.window), "w%d", i);
        assert(am_usage_write(&app, &updated) == AM_USAGE_OK);
    }
    strcpy(updated.window, "overflow");
    assert(am_usage_write(&app, &updated) == AM_USAGE_CAPACITY);
    assert(am_usage_end(&app, "codex", 3) == AM_USAGE_BAD_STATE && app.model.usage_count == 1);
    assert(am_usage_write(&app, &updated) == AM_USAGE_BAD_STATE);
    assert(am_usage_begin(&app, "codex", 4, 100) == AM_USAGE_OK);
    am_usage_expire(&app, 30000100);
    assert(am_usage_end(&app, "codex", 4) == AM_USAGE_BAD_STATE && app.model.usage_count == 1);
    assert(am_usage_begin(&app, "codex", 5, 30000200) == AM_USAGE_OK);
    am_usage_clear(&app, "codex");
    assert(am_usage_end(&app, "codex", 5) == AM_USAGE_BAD_STATE);
}

static void test_brightness(void)
{
    assert(am_brightness_from_percent(0) == 0);
    assert(am_brightness_from_percent(100) == 255);
    assert(am_brightness_from_percent(50) == 128);
    assert(am_brightness_to_percent(192) == 75);
    assert(am_brightness_to_percent(64) == 25);
    for (unsigned percent = 0; percent <= 100; ++percent) {
        assert(am_brightness_to_percent(am_brightness_from_percent(percent)) == percent);
    }
    for (unsigned value = 0; value <= 255; ++value) {
        unsigned percent = am_brightness_to_percent(value);
        assert(percent <= 100);
        if (value) {
            assert(percent >= am_brightness_to_percent(value - 1));
        }
    }
}

static void test_ota_render(const char *directory)
{
    am_app_t app = {0};
    struct am_ui_scene previous = {0};
    struct am_ota_view view = {.state = AM_OTA_RECEIVING, .size = 1000};
    snprintf(view.version, sizeof(view.version), "v2.1");
    const enum am_ota_state states[] = {AM_OTA_RECEIVING, AM_OTA_RECEIVING, AM_OTA_VERIFYING, AM_OTA_READY, AM_OTA_FAILED};
    const char *const names[] = {"ota_start", "ota_progress", "ota_verifying", "ota_ready", "ota_failed"};
    for (unsigned i = 0; i < 5; ++i) {
        view.state = states[i];
        view.received = i ? (i == 1 ? 500 : 1000) : 0;
        snprintf(view.error, sizeof(view.error), "OTA_PENDING_VERIFY");
        memset(&s_scene, 0, sizeof(s_scene));
        s_scene.body_alpha = 255;
        ota_page(&view);
        assert(scene_text("FIRMWARE UPDATE") && scene_text("v2.1"));
        check_incremental(&app, &previous);
        write_ppm(directory, names[i]);
        assert(ota_led(&app, &view, 100000) == ESP_OK);
        if (i == 1) {
            assert(s_ring[7].b == 230 && s_ring[8].b == 6);
        } else if (view.state == AM_OTA_READY) {
            assert(s_ring[0].g == 210 && s_ring[15].g == 210);
        } else if (view.state == AM_OTA_FAILED) {
            assert(s_ring[0].r == 230 && s_ring[15].r == 230);
        }
    }
}

static void test_clock_preview(void)
{
    const char *const states[] = {"working", "thinking", "stale", "task_success", "task_failed", "task_canceled"};
    for (unsigned i = 0; i < sizeof(states) / sizeof(states[0]); ++i) {
        am_model_t model = {.link = AM_LINK_ONLINE, .info_until_us = 20000000};
        struct am_ui_schedule schedule = {0};
        struct am_ui_selection selection;
        model.sessions[0] = session("s1", states[i], "codex");
        am_ui_select(&model, &schedule, 500000, &selection);
        struct am_led_effect before = am_led_select(&model, &selection);
        am_ui_request_clock_preview(&model, 1000000);
        assert(model.clock_preview_until_us == 6000000 && model.info_until_us == 0);
        am_ui_select(&model, &schedule, 1000000, &selection);
        assert(selection.page == AM_PAGE_IDLE && selection.clock_preview && selection.preempted);
        struct am_led_effect after = am_led_select(&model, &selection);
        assert(before.mode == after.mode && strcmp(before.key, after.key) == 0);
        am_ui_select(&model, &schedule, 5999999, &selection);
        assert(selection.page == AM_PAGE_IDLE && selection.clock_preview);
        am_ui_select(&model, &schedule, 6000000, &selection);
        assert(!selection.clock_preview && (selection.page == AM_PAGE_TASK || selection.page == AM_PAGE_RESULT));
        am_ui_request_clock_preview(&model, 7000000);
        am_ui_request_clock_preview(&model, 8000000);
        am_ui_select(&model, &schedule, 12999999, &selection);
        assert(selection.clock_preview);
        am_ui_select(&model, &schedule, 13000000, &selection);
        assert(!selection.clock_preview);
    }
    am_model_t model = {.link = AM_LINK_ONLINE, .usage_supported = true};
    model.usages[0] = (am_usage_t){.used = true, .used_bp = 4200, .window_seconds = 18000};
    strcpy(model.usages[0].provider, "codex");
    struct am_ui_schedule schedule = {0};
    struct am_ui_selection selection;
    am_ui_select(&model, &schedule, 500000, &selection);
    assert(selection.page == AM_PAGE_USAGE);
    am_ui_request_clock_preview(&model, 1000000);
    am_ui_select(&model, &schedule, 1000000, &selection);
    assert(selection.page == AM_PAGE_IDLE && selection.clock_preview);
    am_ui_select(&model, &schedule, 6000000, &selection);
    assert(selection.page == AM_PAGE_USAGE && !selection.clock_preview);
    for (unsigned i = 0; i < 2; ++i) {
        model.sessions[0] = session("s1", i ? "waiting_for_input" : "permission_required", "claude");
        am_ui_request_clock_preview(&model, 7000000);
        assert(model.clock_preview_until_us == 0);
        am_ui_select(&model, &schedule, 7000000, &selection);
        assert(selection.page == AM_PAGE_TASK && selection.group == AM_GROUP_ATTENTION && !selection.clock_preview);
        /* Even a snapshot with an older preview request cannot cover a new urgent session. */
        model.clock_preview_until_us = 12000000;
        am_ui_select(&model, &schedule, 8000000, &selection);
        assert(selection.page == AM_PAGE_TASK && !selection.clock_preview);
    }
    model.sessions[0].used = false;
    model.factory_mode = true;
    am_ui_request_clock_preview(&model, 1000000);
    assert(model.clock_preview_until_us == 0);
}

static void test_session_arcs(const char *directory)
{
    static const char *const s_states[] = {
        "working", "thinking", "waiting_for_input", "permission_required", "stale", "task_success", "task_failed", "task_canceled"
    };
    am_model_t model = {0};
    struct am_ui_selection selection = {.count = AM_MAX_SESSIONS, .current = 4};
    am_app_t app = {0};
    struct am_ui_scene previous = {0};
    for (int i = 0; i < AM_MAX_SESSIONS; ++i) {
        char key[9];
        snprintf(key, sizeof(key), "s%d", i);
        model.sessions[i] = session(key, s_states[i], "codex");
        selection.slots[i] = i;
    }
    struct am_ui_scene first_scene = {0};
    for (int frame = 0; frame < 3; ++frame) {
        memset(&s_scene, 0, sizeof(s_scene));
        s_scene.body_alpha = 255;
        session_indicators(&model, &selection, frame == 1 ? 800000 : 0);
        assert(s_scene.count == AM_MAX_SESSIONS);
        for (int i = 0; i < AM_MAX_SESSIONS; ++i) {
            const struct am_ui_op *op = &s_scene.ops[i];
            struct am_status_animation animation = {0};
            struct am_status_visual visual = am_status_select(&animation, &model.sessions[i], 0, 0);
            assert(op->kind == AM_OP_ARC && op->color == visual.color);
            assert(op->arc_selected == (i == 4));
            assert(op->y >= 154);
            if (i < 5) {
                assert(op->x + op->width < 76);
            } else {
                assert(op->x > 154 && op->y > 180);
            }

        }
        if (frame == 0) {
            first_scene = s_scene;
        } else {
            assert(memcmp(&first_scene, &s_scene, sizeof(s_scene)) == 0);
        }
        assert(am_scene_render(&app, &previous, &s_scene, false) == ESP_OK);
        check_incremental(&app, &previous);
        if (frame == 1) {
            write_ppm(directory, "session_arc_eight");
        }
    }
    static const int s_angles[] = {154, 146, 138, 130, 122, 46, 54, 62};
    static const int s_expected[AM_MAX_SESSIONS][AM_MAX_SESSIONS] = {
        {2}, {1, 2}, {1, 2, 3}, {0, 1, 2, 3}, {0, 1, 2, 3, 4},
        {0, 1, 2, 3, 4, 6}, {0, 1, 2, 3, 4, 5, 6}, {0, 1, 2, 3, 4, 5, 6, 7}
    };
    for (int count = AM_MAX_SESSIONS; count >= 0; --count) {
        for (int i = 0; i < AM_MAX_SESSIONS; ++i) {
            model.sessions[i].used = i < count;
        }
        memset(&s_scene, 0, sizeof(s_scene));
        s_scene.body_alpha = 255;
        selection.count = count;
        selection.current = 0;
        session_indicators(&model, &selection, 800000);
        assert(s_scene.count == count);
        for (int i = 0; i < count; ++i) {
            const struct am_ui_op *op = &s_scene.ops[i];
            int position = s_expected[count - 1][i];
            assert(op->arc_angle == s_angles[position]);
            assert(op->arc_selected == (i == selection.current));
        }
        assert(am_scene_render(&app, &previous, &s_scene, false) == ESP_OK);
        check_incremental(&app, &previous);
    }
    static const char *const s_inactive[] = {"initializing", "ready", "ended", "offline"};
    for (unsigned state = 0; state < sizeof(s_inactive) / sizeof(s_inactive[0]); ++state) {
        /* Inactive entries before and between visible entries must not consume a position. */
        for (int i = 0; i < AM_MAX_SESSIONS; ++i) {
            model.sessions[i] = session("off", s_inactive[state], "codex");
            assert(!am_session_displayable(&model.sessions[i]));
        }
        model.link = AM_LINK_ONLINE;
        struct am_ui_schedule schedule = {0};
        model.sessions[2] = session("work", "working", "codex");
        model.sessions[6] = session("input", "waiting_for_input", "codex");
        memset(&s_scene, 0, sizeof(s_scene));
        s_scene.body_alpha = 255;
        am_ui_select(&model, &schedule, 800000, &selection);
        session_indicators(&model, &selection, 800000);
        assert(s_scene.count == 2 && selection.slots[0] == 6);
        assert(s_scene.ops[0].arc_angle == 146 && !s_scene.ops[0].arc_selected);
        assert(s_scene.ops[1].arc_angle == 138 && s_scene.ops[1].arc_selected);
        assert(s_scene.ops[1].color == TFT_RGB565(255, 176, 46));
        assert(am_scene_render(&app, &previous, &s_scene, false) == ESP_OK);
        check_incremental(&app, &previous);
        model.sessions[6] = session("input", s_inactive[state], "codex");
        memset(&s_scene, 0, sizeof(s_scene));
        s_scene.body_alpha = 255;
        am_ui_select(&model, &schedule, 800000, &selection);
        session_indicators(&model, &selection, 800000);
        assert(s_scene.count == 1 && s_scene.ops[0].arc_selected);
        assert(s_scene.ops[0].arc_angle == 138);
        assert(am_scene_render(&app, &previous, &s_scene, false) == ESP_OK);
        check_incremental(&app, &previous);
        model.sessions[2] = session("work", s_inactive[state], "codex");
        memset(&s_scene, 0, sizeof(s_scene));
        am_ui_select(&model, &schedule, 800000, &selection);
        session_indicators(&model, &selection, 800000);
        assert(s_scene.count == 0);
    }

}

static void test_session_dot_rotation(void)
{
    am_model_t model = {.link = AM_LINK_ONLINE};
    model.sessions[0] = session("done1", "task_success", "codex");
    model.sessions[3] = session("done2", "task_success", "codex");
    model.sessions[6] = session("work", "working", "codex");
    struct am_ui_schedule schedule = {0};
    struct am_ui_selection selection;
    am_app_t app = {0};
    struct am_ui_scene previous = {0};
    for (int step = 0; step < 7; ++step) {
        if (step == 2) {
            model.sessions[0] = session("done1", "ended", "codex");
        } else if (step == 3) {
            model.sessions[3] = session("done2", "ready", "codex");
        } else if (step == 4) {
            model.sessions[1] = session("input", "waiting_for_input", "claude");
            model.sessions[2] = session("approve", "permission_required", "codex");
        } else if (step == 5) {
            model.sessions[1].used = false;
        } else if (step == 6) {
            model.sessions[2].used = false;
            model.sessions[6].used = false;
        }
        int64_t now = 800000 + step * 16000000LL;
        am_ui_select(&model, &schedule, now, &selection);
        static const int s_counts[] = {2, 2, 1, 1, 2, 1, 0};
        assert(selection.count == s_counts[step]);
        if (step < 2) {
            assert(selection.group == AM_GROUP_RESULT);
            assert(selection.slots[selection.current] == (step ? 3 : 0));
        } else if (step == 3) {
            assert(selection.group == AM_GROUP_WORK && selection.slots[0] == 6);
        }
        memset(&s_scene, 0, sizeof(s_scene));
        s_scene.body_alpha = 255;
        session_indicators(&model, &selection, now);
        static const int s_indicator_counts[] = {3, 3, 2, 1, 3, 2, 0};
        static const int s_indicator_slots[][3] = {{0, 3, 6}, {0, 3, 6}, {3, 6}, {6}, {1, 2, 6}, {2, 6}, {0}};
        int count = s_indicator_counts[step];
        assert(s_scene.count == count);
        for (int i = 0; i < count; ++i) {
            int slot = s_indicator_slots[step][i];
            struct am_status_animation animation = {0};
            struct am_status_visual visual = am_status_select(&animation, &model.sessions[slot], 0, now);
            assert(s_scene.ops[i].color == visual.color);
            assert(s_scene.ops[i].arc_selected == (slot == selection.slots[selection.current]));
        }
        check_incremental(&app, &previous);
        if (selection.count) {
            memset(&s_scene, 0, sizeof(s_scene));
            s_scene.body_alpha = 255;
            session_page(&model, &model.sessions[selection.slots[selection.current]], &selection, now, 0, 1792854000);
            int dots = 0;
            for (int i = 0; i < s_scene.count; ++i) {
                if (s_scene.ops[i].kind == AM_OP_ARC) {
                    dots++;
                }
            }
            assert(dots == count);
            check_incremental(&app, &previous);
        }
    }
}

static void test_session_completion_count(void)
{
    am_model_t model = {.link = AM_LINK_ONLINE};
    struct am_ui_schedule schedule = {0};
    struct am_ui_selection selection;
    am_app_t app = {0};
    struct am_ui_scene previous = {0};
    static const char *const s_states[][2] = {
        {"working", "working"}, {"working", "task_success"}, {"working", "ready"},
        {"working", "working"}, {"task_success", "task_success"}, {"task_success", "ended"}, {"ended", "ended"}
    };
    static const int s_counts[] = {2, 2, 1, 2, 2, 1, 0};
    model.sessions[0] = session("work1", "working", "codex");
    model.sessions[4] = session("work2", "working", "codex");
    for (int step = 0; step < 7; ++step) {
        strcpy(model.sessions[0].state, s_states[step][0]);
        strcpy(model.sessions[4].state, s_states[step][1]);
        int64_t now = 1000000 + step * 1000000LL;
        am_ui_select(&model, &schedule, now, &selection);
        memset(&s_scene, 0, sizeof(s_scene));
        s_scene.body_alpha = 255;
        session_indicators(&model, &selection, now);
        assert(s_scene.count == s_counts[step]);
        if (step == 1) {
            assert(selection.count == 1 && selection.group == AM_GROUP_RESULT && selection.slots[0] == 4);
            assert(!s_scene.ops[0].arc_selected && s_scene.ops[1].arc_selected);
            assert(s_scene.ops[0].color == TFT_RGB565(54, 226, 154));
            assert(s_scene.ops[0].arc_angle == 146 && s_scene.ops[1].arc_angle == 138);
        } else if (step == 2) {
            assert(selection.count == 1 && selection.group == AM_GROUP_WORK && selection.slots[0] == 0);
            assert(s_scene.ops[0].arc_selected && s_scene.ops[0].arc_angle == 138);
        }
        check_incremental(&app, &previous);
    }
}

static void test_session_arc_glow(void)
{
    am_app_t app = {0};
    struct am_ui_scene previous = {0};
    uint16_t normal[240 * 240];
    memset(&s_scene, 0, sizeof(s_scene));
    s_scene.body_alpha = 255;
    am_scene_session_arc(&s_scene, 2, TFT_RGB565(139, 153, 255), false);
    check_incremental(&app, &previous);
    memcpy(normal, s_screen, sizeof(normal));
    memset(&s_scene, 0, sizeof(s_scene));
    s_scene.body_alpha = 255;
    am_scene_session_arc(&s_scene, 2, TFT_RGB565(139, 153, 255), true);
    check_incremental(&app, &previous);
    int normal_pixels = 0, selected_pixels = 0, fringe = 0;
    for (int i = 0; i < 240 * 240; ++i) {
        normal_pixels += normal[i] != 0;
        selected_pixels += s_screen[i] != 0;
        fringe += normal[i] == 0 && s_screen[i] != 0;
    }
    assert(selected_pixels > normal_pixels && fringe > 0);
    assert(s_screen[192 * 240 + 40] != normal[192 * 240 + 40]);
    s_submitted_pixels = 0;
    assert(am_scene_render(&app, &previous, &s_scene, false) == ESP_OK);
    assert(s_submitted_pixels == 0);
    /* Every left/right selection change, including glow removal, must match a complete repaint. */
    for (int selected = 0; selected < AM_MAX_SESSIONS; ++selected) {
        memset(&s_scene, 0, sizeof(s_scene));
        s_scene.body_alpha = 255;
        for (int i = 0; i < AM_MAX_SESSIONS; ++i) {
            am_scene_session_arc(&s_scene, i, TFT_RGB565(54, 226, 154), i == selected);
        }
        check_incremental(&app, &previous);
    }
}

int main(int argc,char **argv)
{
    assert(argc == 2);
    test_session_arcs(argv[1]);
    test_session_arc_glow();
    test_session_completion_count();
    test_session_dot_rotation();
    test_clock_preview();
    test_usage_batches();
    test_brightness();
    test_text();
    test_directory_fonts();
    test_selection();
    test_full_session_rotation();
    test_idle_rotation();
    test_standby_usage_availability();
    test_calendar_and_led();
    test_led_patterns();
    write_led_preview(argv[1]);
    test_usage_colors();
    test_session_quota(argv[1]);
    test_detail_fairness();
    test_detail_timing();
    test_render(argv[1]);
    test_ota_render(argv[1]);
    test_v62_pages(argv[1]);
    test_clock_themes(argv[1]);
    test_idle_clock(argv[1]);
    test_session_clock(argv[1]);
    test_status_animation(argv[1]);
    puts("UI text, scheduler, partial rendering, factory fonts, LED and animated status checks passed");
    return 0;
}
