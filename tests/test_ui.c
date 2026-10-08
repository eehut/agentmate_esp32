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
    am_ui_select(&app.model, &schedule, 55000000, &out); assert(out.page == AM_PAGE_USAGE);
    for (int i = 1; i < 5; ++i) {
        entry.used_bp = i * 100;
        assert(am_usage_write(&app, &entry) == AM_USAGE_OK);
        am_ui_select(&app.model, &schedule, 55000000 + i * 1000000, &out);
        assert(out.page == AM_PAGE_USAGE && schedule.page_since_us == 55000000);
    }
    am_ui_select(&app.model, &schedule, 65000000, &out); assert(out.page == AM_PAGE_IDLE);
    am_usage_clear(&app, "codex"); assert(app.model.usage_supported);
    am_ui_select(&app.model, &schedule, 75000000, &out);
    assert(out.page == AM_PAGE_USAGE && out.usage[0] == -1 && out.usage[1] == -1);
    app.model.sessions[0] = session("s1", "permission_required", "claude");
    am_ui_select(&app.model, &schedule, 76000000, &out); assert(out.page == AM_PAGE_TASK);
    strcpy(app.model.sessions[0].state, "task_success");
    am_ui_select(&app.model, &schedule, 77000000, &out); assert(out.page == AM_PAGE_RESULT);
    strcpy(app.model.sessions[0].state, "ready");
    am_ui_select(&app.model, &schedule, 78000000, &out); assert(out.page == AM_PAGE_USAGE);
    am_ui_select(&app.model, &schedule, 88000000, &out); assert(out.page == AM_PAGE_IDLE);
    app.model.usage_supported = false; app.model.remote_generation++;
    am_ui_select(&app.model, &schedule, 89000000, &out); assert(out.page == AM_PAGE_IDLE);
    assert(am_usage_write(&app, &entry) == AM_USAGE_OK);
    am_ui_select(&app.model, &schedule, 90000000, &out); assert(out.page == AM_PAGE_USAGE);
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
                    if (alpha > 3 && dx * dx + dy * dy > 216 * 216) {
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

static const struct am_ui_op *project_op(void)
{
    for (int i = 0; i < s_scene.count; ++i) {
        if (s_scene.ops[i].kind == AM_OP_TEXT && s_scene.ops[i].y == 84) {
            return &s_scene.ops[i];
        }
    }
    assert(0);
    return NULL;
}

static void test_directory_fonts(void)
{
    am_model_t model = {0};
    am_session_t current = session("s1", "working", "codex");
    struct am_ui_selection selection = {.count = 1};
    const char *names[] = {"agentmate", "工程目录", "agentmate-provider-registry-dashboard", ""};
    for (int i = 0; i < 4; ++i) {
        strcpy(current.project_name, names[i]);
        memset(&s_scene, 0, sizeof(s_scene));
        session_page(&model, &current, &selection, 1000000, 0);
        if (i == 3) {
            continue;
        }
        const struct am_ui_op *op = project_op();
        assert(op->font == (i == 2 ? &g_am_font_directory : &g_am_font_directory_large));
        if (i < 2) {
            assert(strcmp(op->text, names[i]) == 0);
        } else {
            assert(strstr(op->text, "…"));
        }
        assert(op->width <= 137);
        assert(s_scene.ops[2].kind == AM_OP_RECT && s_scene.ops[2].color == C_CODEX_TITLE);
    }
    char name[65] = "";
    while (am_text_width(&g_am_font_directory_large, name) + am_text_width(&g_am_font_directory_large, "W") <= 137) {
        strcat(name, "W");
    }
    strcpy(current.project_name, name);
    memset(&s_scene, 0, sizeof(s_scene)); session_page(&model, &current, &selection, 1000000, 0);
    assert(project_op()->font == &g_am_font_directory_large);
    strcat(name, "W"); strcpy(current.project_name, name);
    memset(&s_scene, 0, sizeof(s_scene)); session_page(&model, &current, &selection, 1000000, 0);
    assert(project_op()->font == &g_am_font_directory);
    assert(am_text_ink_width(&g_am_font_directory, "工程目录") < am_text_ink_width(&g_am_font_directory_large, "工程目录"));
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
            if (op->kind == AM_OP_TEXT && op->y == 130) {
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
    static const struct {
        int used_bp;
        const char *text;
        uint16_t color;
    } s_cases[] = {
        {0, "100", C_MINT}, {4200, "58", C_MINT}, {5000, "50", C_MINT}, {5050, "50", C_MINT},
        {5051, "49", C_AMBER}, {6500, "35", C_AMBER}, {8000, "20", C_AMBER}, {8050, "20", C_AMBER},
        {8051, "19", C_CORAL}, {9200, "8", C_CORAL}, {9950, "1", C_CORAL}, {9951, "0", C_CORAL},
        {10000, "0", C_CORAL}, {-1, "--", C_MUTED},
    };
    am_app_t app = {0};
    am_model_t *model = &app.model;
    model->link = AM_LINK_ONLINE;
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
    model->usages[2] = model->usages[0];
    strcpy(model->usages[2].metric, "a");
    model->usages[3] = model->usages[2];
    strcpy(model->usages[3].window, "aaa");
    am_ui_select(model, &schedule, 1000000, &selection);
    assert(selection.usage[0] == 3 && selection.usage[1] == 1);
    strcpy(model->usages[3].provider, "claude");
    am_ui_select(model, &schedule, 1000000, &selection);
    assert(selection.usage[0] == 2 && selection.usage[1] == 1);
    int64_t page_since = schedule.page_since_us;
    struct am_ui_scene previous = {0};
    for (size_t i = 0; i < sizeof(s_cases) / sizeof(s_cases[0]); ++i) {
        model->usages[2].used_bp = s_cases[i].used_bp;
        model->usages[1].used_bp = 9200;
        am_ui_select(model, &schedule, 1000000, &selection);
        assert(selection.page == AM_PAGE_TASK && schedule.page_since_us == page_since);
        memset(&s_scene, 0, sizeof(s_scene));
        s_scene.body_alpha = 255;
        business_header(true);
        session_page(model, &model->sessions[0], &selection, 1000000, 0);
        assert(s_scene.count < AM_UI_OPS);
        const struct am_ui_op *bar = &s_scene.ops[s_scene.count - 4];
        const struct am_ui_op *value = bar + 1;
        assert(bar->kind == AM_OP_PROGRESS && bar->x == 31 && bar->y == 150 && bar->width == 28 && bar->height == 3);
        assert(bar->remaining_bp == (s_cases[i].used_bp < 0 ? 0 : 10000 - s_cases[i].used_bp));
        assert(bar->color == s_cases[i].color && bar->track_color == C_LINE && bar->radius == 1);
        assert(value->kind == AM_OP_TEXT && strcmp(value->text, s_cases[i].text) == 0 && value->color == s_cases[i].color);
        assert(value->x >= 32 && value->x + value->width <= 58 && value->y == 131 && value->font == &g_am_font_label);
        assert(value->height == 17 && value->y + value->height < bar->y);
        assert((bar + 2)->kind == AM_OP_PROGRESS && (bar + 2)->y == 182);
        assert(strcmp((bar + 3)->text, "8") == 0);
        assert((bar + 3)->y == 163 && (bar + 3)->font == &g_am_font_label && (bar + 3)->height == 17);
        s_submitted_pixels = 0;
        assert(am_scene_render(&app, &previous, &s_scene, i == 0) == ESP_OK);
        if (i) {
            assert(s_submitted_pixels < 2500);
        }
        check_incremental(&app, &previous);
        if (i == 0 || i == 1 || i == 5 || i == 12 || i == 13) {
            char name[48];
            snprintf(name, sizeof(name), "codex_quota_%s", s_cases[i].text);
            write_ppm(directory, name);
        }
    }
    /* Pending batches stay invisible; completion updates the active session atomically. */
    am_usage_t updated = model->usages[2];
    updated.used_bp = 6500;
    assert(am_usage_begin(&app, "codex", 1, 1000000) == AM_USAGE_OK);
    assert(am_usage_write(&app, &updated) == AM_USAGE_OK);
    assert(model->usages[2].used_bp == -1);
    assert(am_usage_end(&app, "codex", 1) == AM_USAGE_OK);
    am_ui_select(model, &schedule, 1000000, &selection);
    assert(selection.usage[0] >= 0 && selection.usage[1] == -1 && selection.page == AM_PAGE_TASK);
    memset(&s_scene, 0, sizeof(s_scene)); s_scene.body_alpha = 255; business_header(true);
    session_page(model, &model->sessions[0], &selection, 1000000, 0);
    assert(strcmp(s_scene.ops[s_scene.count - 3].text, "35") == 0 && strcmp(s_scene.ops[s_scene.count - 1].text, "--") == 0);
    check_incremental(&app, &previous); write_ppm(directory, "codex_quota_single");
    am_usage_clear(&app, "codex");
    am_ui_select(model, &schedule, 1000000, &selection);
    assert(selection.usage[0] == -1 && selection.usage[1] == -1 && selection.page == AM_PAGE_TASK);
    memset(&s_scene, 0, sizeof(s_scene)); s_scene.body_alpha = 255; business_header(true);
    session_page(model, &model->sessions[0], &selection, 1000000, 0);
    assert(strcmp(s_scene.ops[s_scene.count - 3].text, "--") == 0 && strcmp(s_scene.ops[s_scene.count - 1].text, "--") == 0);
    check_incremental(&app, &previous); write_ppm(directory, "codex_quota_missing");
    static const char *const s_states[] = {"working", "thinking", "waiting_for_input", "permission_required", "stale",
                                         "task_success", "task_failed", "task_canceled"};
    selection.count = AM_MAX_SESSIONS;
    for (size_t i = 0; i < sizeof(s_states) / sizeof(s_states[0]); ++i) {
        strcpy(model->sessions[0].state, s_states[i]);
        memset(&s_scene, 0, sizeof(s_scene)); s_scene.body_alpha = 255; business_header(true);
        session_page(model, &model->sessions[0], &selection, 1000000, 0);
        assert(s_scene.count < AM_UI_OPS && s_scene.ops[s_scene.count - 4].kind == AM_OP_PROGRESS);
        check_incremental(&app, &previous);
    }
    /* Context rotation uses the selected session clock, independent of data revisions. */
    memset(&schedule, 0, sizeof(schedule));
    strcpy(model->sessions[0].state, "working");
    model->sessions[0].context_mode = 2;
    model->sessions[0].ctx_pct_bp = 2500;
    model->sessions[0].ctx_used = 50000;
    model->sessions[0].ctx_window = 200000;
    am_ui_select(model, &schedule, 1000000, &selection);
    assert(!selection.show_context);
    am_ui_select(model, &schedule, 5999999, &selection);
    assert(!selection.show_context);
    model->revision++;
    am_ui_select(model, &schedule, 6000000, &selection);
    assert(selection.show_context && schedule.page_since_us == 1000000);
    memset(&s_scene, 0, sizeof(s_scene)); s_scene.body_alpha = 255; business_header(true);
    session_page(model, &model->sessions[0], &selection, 6000000, 0);
    assert(strcmp(s_scene.ops[s_scene.count - 3].text, "25.00%") == 0);
    assert(strcmp(s_scene.ops[s_scene.count - 2].text, "50k") == 0);
    assert(strcmp(s_scene.ops[s_scene.count - 1].text, "200k") == 0);
    check_incremental(&app, &previous); write_ppm(directory, "codex_context_known");
    model->sessions[0].context_mode = 1;
    memset(&s_scene, 0, sizeof(s_scene)); s_scene.body_alpha = 255; business_header(true);
    session_page(model, &model->sessions[0], &selection, 6000000, 0);
    for (int i = 1; i <= 3; ++i) {
        assert(strcmp(s_scene.ops[s_scene.count - i].text, "--") == 0);
    }
    check_incremental(&app, &previous); write_ppm(directory, "codex_context_unknown");
    model->sessions[0].context_mode = 0;
    memset(&s_scene, 0, sizeof(s_scene)); s_scene.body_alpha = 255; business_header(true);
    session_page(model, &model->sessions[0], &selection, 6000000, 0);
    assert(s_scene.ops[s_scene.count - 4].kind == AM_OP_PROGRESS);
    check_incremental(&app, &previous);
    am_ui_select(model, &schedule, 11000000, &selection);
    assert(!selection.show_context);
    model->sessions[1] = session("s2", "working", "codex");
    am_ui_select(model, &schedule, 11000000, &selection);
    assert(selection.current == 1 && !selection.show_context);
    am_ui_select(model, &schedule, 16000000, &selection);
    assert(selection.show_context);
    strcpy(model->sessions[1].state, "task_success");
    am_ui_select(model, &schedule, 16000001, &selection);
    assert(selection.page == AM_PAGE_RESULT && !selection.show_context);
    model->remote_generation++;
    am_ui_select(model, &schedule, 22000000, &selection);
    assert(!selection.show_context);
    memset(&model->sessions[1], 0, sizeof(model->sessions[1]));
    strcpy(model->sessions[0].provider, "claude");
    model->sessions[0].context_mode = 2;
    model->sessions[0].ctx_pct_bp = 600;
    memset(&s_scene, 0, sizeof(s_scene)); s_scene.body_alpha = 255; business_header(true);
    session_page(model, &model->sessions[0], &selection, 1000000, 0);
    for (int i = 0; i < s_scene.count; ++i) {
        assert(s_scene.ops[i].kind != AM_OP_PROGRESS);
    }
    assert(strcmp(s_scene.ops[s_scene.count - 3].text, "6.00%") == 0);
    check_incremental(&app, &previous);
    /* Inspect left-to-right progress independently of the unchanged numeric font. */
    const unsigned portions[] = {0, 2500, 5000, 7500, 10000};
    for (size_t i = 0; i < sizeof(portions) / sizeof(portions[0]); ++i) {
        memset(&s_scene, 0, sizeof(s_scene)); s_scene.body_alpha = 255;
        am_scene_progress(&s_scene, 31, 147, 28, 3, C_MINT, C_LINE, portions[i]);
        check_incremental(&app, &previous);
        assert(s_screen[146 * 240 + 45] == AM_UI_BG && s_screen[150 * 240 + 45] == AM_UI_BG);
        for (int x = 0; x < 28; ++x) {
            assert(s_screen[148 * 240 + 31 + x] == (x < (int)i * 7 ? C_MINT : C_LINE));
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
            memset(&s_scene, 0, sizeof(s_scene)); s_scene.body_alpha = 255; business_header(true);
            session_page(&model, &current, &selection, 1000000 + frame * AM_STATUS_TICK_US, i + 10);
            const struct am_ui_op *border = NULL;
            const struct am_ui_op *label = NULL;
            for (int op = 0; op < s_scene.count; ++op) {
                if (s_scene.ops[op].kind == AM_OP_STATUS_FRAME) {
                    border = &s_scene.ops[op]; label = &s_scene.ops[op + 1];
                }
            }
            assert(border && border->animation_frame == frame % s_periods[i] && border->color == s_colors[i]);
            assert(label->kind == AM_OP_TEXT && label->color == s_colors[i] && strstr(label->text, "…") == NULL);
            assert(label->x >= 88 && label->x + label->width <= 205);
            s_submitted_pixels = 0;
            assert(am_scene_render(&app, &previous, &s_scene, frame == 0) == ESP_OK);
            if (frame) {
                assert(s_submitted_pixels >= AM_STATUS_WIDTH * AM_STATUS_HEIGHT);
                assert(s_submitted_pixels <= AM_STATUS_WIDTH * AM_STATUS_HEIGHT + 5000);
                bool changed = false;
                for (int y = 110; y < 158; ++y) {
                    for (int x = 75; x < 218; ++x) {
                        changed |= s_screen[y * 240 + x] != previous_pixels[y * 240 + x];
                    }
                }
                assert(changed);
                for (int y = 120; y < 148; ++y) {
                    assert(memcmp(first + y * 240 + 88, s_screen + y * 240 + 88, 117 * sizeof(uint16_t)) == 0);
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
        /* All frozen states repeat exactly after their period. */
        if (!am_state_advancing(current.state)) {
            assert(memcmp(first, s_screen, sizeof(first)) == 0);
        }
        s_submitted_pixels = 0;
        assert(am_scene_render(&app, &previous, &s_scene, false) == ESP_OK && s_submitted_pixels == 0);
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
            assert(s_submitted_pixels == 15 * 49);
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
            assert(changed > 0 && changed < 150 && right - left < 10);
            /* Digit ink occupies y=99..131; separator center must align within one pixel. */
            assert(abs(top + bottom - 230) <= 2);
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
        memset(&s_scene,0,sizeof(s_scene)); s_scene.body_alpha=255; business_header(true);
        strcpy(s.state,states[i]); session_page(&model, &s,&sel,s_now, 0); check_incremental(&app,&previous); write_ppm(directory,states[i]);
    }
    strcpy(s.host_label, "WWWWWWWW");
    strcpy(s.session_short_name, "S4294967295");
    memset(&s_scene, 0, sizeof(s_scene)); s_scene.body_alpha = 255; business_header(true); session_page(&model, &s, &sel, s_now, 0);
    bool identity_found = false;
    for (int i = 0; i < s_scene.count; ++i) {
        if (s_scene.ops[i].kind == AM_OP_TEXT && strcmp(s_scene.ops[i].text, "WWWWWWWW·S4294967295") == 0) {
            identity_found = true;
            assert(s_scene.ops[i].x >= 78 && s_scene.ops[i].x + s_scene.ops[i].width <= 215);
        }
    }
    assert(identity_found);
    check_incremental(&app, &previous); write_ppm(directory, "long_host_title");
    strcpy(s.host_label, "WSL"); strcpy(s.session_short_name, "S1");
    sel.count = AM_MAX_SESSIONS; sel.current = AM_MAX_SESSIONS - 1;
    memset(&s_scene, 0, sizeof(s_scene)); s_scene.body_alpha = 255; business_header(true); session_page(&model, &s, &sel, s_now, 0);
    check_incremental(&app, &previous); write_ppm(directory, "eight_sessions");
    sel.count = 2; sel.current = 0;
    const char *projects[] = {"agentmate", "工程目录", "agentmate-provider-registry-dashboard"};
    const char *fixtures[] = {"codex_short_directory", "codex_chinese_directory", "codex_long_directory"};
    am_session_t codex = session("s1", "working", "codex");
    for (int i = 0; i < 3; ++i) {
        strcpy(codex.project_name, projects[i]);
        memset(&s_scene, 0, sizeof(s_scene)); s_scene.body_alpha = 255; business_header(true); session_page(&model, &codex, &sel, s_now, 0);
        check_incremental(&app, &previous); write_ppm(directory, fixtures[i]);
    }
    strcpy(s.state,"working");
    memset(&s_scene,0,sizeof(s_scene)); s_scene.body_alpha=255; business_header(true); session_page(&model, &s,&sel,s_now, 0);
    check_incremental(&app,&previous); s_submitted_pixels=0;
    memset(&s_scene,0,sizeof(s_scene)); s_scene.body_alpha=255; business_header(true); session_page(&model, &s,&sel,s_now+1000000, 0);
    am_scene_render(&app,&previous,&s_scene,false); assert(s_submitted_pixels <= AM_STATUS_WIDTH * AM_STATUS_HEIGHT + 5000);
    s.context_mode=1; strcpy(s.project_name,"工程目录-agentmate-provider-registry-dashboard"); strcpy(s.key,"ABCDEFGH"); s.task_seconds=UINT32_MAX;
    memset(&s_scene,0,sizeof(s_scene)); s_scene.body_alpha=255; business_header(true); session_page(&model, &s,&sel,s_now, 0);
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
        assert(op->kind != AM_OP_RECT || op->height != 1);
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

int main(int argc,char **argv)
{
    assert(argc == 2);
    test_usage_batches();
    test_brightness();
    test_text();
    test_directory_fonts();
    test_selection();
    test_full_session_rotation();
    test_idle_rotation();
    test_calendar_and_led();
    test_led_patterns();
    write_led_preview(argv[1]);
    test_usage_colors();
    test_session_quota(argv[1]);
    test_render(argv[1]);
    test_idle_clock(argv[1]);
    test_status_animation(argv[1]);
    puts("UI text, scheduler, partial rendering, factory fonts, LED and animated status checks passed");
    return 0;
}
