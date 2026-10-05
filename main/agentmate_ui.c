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
# Description: AgentMate circular TFT and RGB LED user interface
# Author: Liu Chuansen <liuchuansen@outlook.com>
#
#####################################################################################
#
# Revision History:
# 2026-10-05 3.1      Codex         Use a middle dot between host and session names.
# 2026-10-04 3.0      Codex         Add protocol 2 host labels and session display names.
#
# Date       Version  Author       Description
# ---------- -------- ------------ -----------------------------------------------
# 2026-10-01 1.0      Liu Chuansen  Organized for project C style.
# 2026-10-01 1.1      Liu Chuansen  Added circle-aware partial screen updates.
# 2026-10-01 1.2      Codex         Added factory display and font diagnostics.
# 2026-10-02 1.3      Codex         Display transient task states and thinking.
# 2026-10-02 2.0      Codex         Implemented phase-one visual pages and independent LED effects.
# 2026-10-03 2.1      Codex         Share percentage conversion with AT settings.
# 2026-10-03 2.7      Codex         Replace status icons with centered animated rounded borders.
# 2026-10-03 2.6      Codex         Follow selected sessions with binary LED patterns and ten-second rotation.
# 2026-10-03 2.5      Codex         Color remaining quota by the displayed percentage.
# 2026-10-03 2.4      Codex         Darken Codex titles and adapt directory and idle heading sizes.
# 2026-10-03 2.3      Codex         Render cleared quota placeholders during standby rotation.
# 2026-10-03 2.2      Codex         Improve quota headings and reset layout.
# 2026-10-03 2.8      Codex         Added deterministic LED v2 patterns and frame diagnostics.
# 2026-10-04 2.9      Codex         Center and blink the standby clock separator independently.
#
#####################################################################################
*/

#include "agentmate_app.h"

#include <ctype.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "esp_timer.h"
#include "freertos/task.h"
#include "lcd_fonts.h"
#include "provider_icons.h"
#include "agentmate_ui_state.h"
#include "agentmate_led.h"
#include "agentmate_ui_render.h"

#define BG TFT_RGB565(12, 17, 28)
#define FG TFT_RGB565(234, 240, 249)
#define MUTED TFT_RGB565(125, 143, 163)
#define ACCENT TFT_RGB565(84, 203, 190)
#define WARNING TFT_RGB565(255, 185, 72)
#define UI_SIZE 240
#define UI_CENTER_TWICE (UI_SIZE - 1)
#define UI_SAFE_RADIUS_TWICE 232
#define UI_FONT_WIDTH 8
#define UI_FONT_HEIGHT 16
#define UI_MAX_LINES 16
#define UI_CENTERED_X -1

typedef struct {
    int x;
    int y;
    int clear_x;
    int right;
    uint16_t color;
    char text[64];
} ui_line_t;

typedef struct {
    ui_line_t lines[UI_MAX_LINES];
    int line_count;
} ui_frame_t;

static ui_frame_t s_next_frame;

static bool circle_span(int y, int height, int *left, int *right)
{
    if (y < 0 || height <= 0 || y + height > UI_SIZE) {
        return false;
    }
    int safe_left = 0;
    for (int row = y; row < y + height; ++row) {
        int dy = row * 2 - UI_CENTER_TWICE;
        int row_left = 0;
        while (row_left < UI_SIZE / 2) {
            int dx = row_left * 2 - UI_CENTER_TWICE;
            if (dx * dx + dy * dy <= UI_SAFE_RADIUS_TWICE * UI_SAFE_RADIUS_TWICE) {
                break;
            }
            row_left++;
        }
        if (row_left >= UI_SIZE / 2) {
            return false;
        }
        if (row_left > safe_left) {
            safe_left = row_left;
        }
    }
    *left = safe_left;
    *right = UI_CENTER_TWICE - safe_left;
    return true;
}

static void label(int requested_x, int y, const char *value, uint16_t color)
{
    if (s_next_frame.line_count >= UI_MAX_LINES) {
        return;
    }
    int left;
    int right;
    if (!circle_span(y, UI_FONT_HEIGHT, &left, &right)) {
        return;
    }
    int clear_x = requested_x == UI_CENTERED_X ? left : requested_x;
    if (clear_x < left) {
        clear_x = left;
    }
    if (clear_x > right) {
        return;
    }
    size_t count = strlen(value);
    size_t capacity = (size_t)(right - clear_x + 1) / UI_FONT_WIDTH;
    if (count > capacity) {
        count = capacity;
    }
    ui_line_t *line = &s_next_frame.lines[s_next_frame.line_count++];
    if (count >= sizeof(line->text)) {
        count = sizeof(line->text) - 1;
    }
    memcpy(line->text, value, count);
    line->text[count] = 0;
    line->clear_x = clear_x;
    line->right = right;
    line->x = requested_x == UI_CENTERED_X ? (UI_SIZE - (int)count * UI_FONT_WIDTH) / 2 : clear_x;
    line->y = y;
    line->color = color;
}

static esp_err_t draw_line(am_app_t *app, const ui_line_t *line)
{
    int width = line->right - line->clear_x + 1;
    size_t pixels = (size_t)width * UI_FONT_HEIGHT;
    for (size_t i = 0; i < pixels; ++i) {
        g_am_ui_pixels[i] = BG;
    }
    const lcd_font_t *font = &g_lcd_font_ascii_8x16;
    size_t count = strlen(line->text);
    int text_x = line->x - line->clear_x;
    for (size_t column = 0; column < count; ++column) {
        unsigned char ch = (unsigned char)line->text[column];
        const uint8_t *glyph = font->get_code_data(font, ch >= 32 && ch <= 126 ? ch : '?');
        for (int row = 0; row < UI_FONT_HEIGHT; ++row) {
            uint8_t bits = glyph ? glyph[row] : 0;
            for (int bit = 0; bit < UI_FONT_WIDTH; ++bit) {
                if (bits & (0x80U >> bit)) {
                    g_am_ui_pixels[(size_t)row * width + text_x + (int)column * UI_FONT_WIDTH + bit] = line->color;
                }
            }
        }
    }
    return tft_display_draw_bitmap_rgb565(app->display, line->clear_x, line->y, width, UI_FONT_HEIGHT, g_am_ui_pixels);
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

static const lcd_font_t *factory_font(uint8_t index)
{
    static const lcd_font_t *const s_fonts[] = {
        &g_lcd_font_ascii_8x8, &g_lcd_font_ascii_8x16, &g_lcd_font_ascii_10x18,
        &g_lcd_font_sun_ascii_12x22, &g_lcd_font_ter_ascii_16x32,
    };
    return s_fonts[index < 5 ? index : 0];
}

static esp_err_t render_font_sample(am_app_t *app, uint8_t index, int page)
{
    const lcd_font_t *font = factory_font(index);
    esp_err_t err = tft_display_fill(app->display, BG);
    if (err != ESP_OK) {
        return err;
    }
    int rows = (UI_SIZE - 48) / (font->height + 4);
    int columns = (UI_SIZE - 56) / font->width;
    int bytes_per_row = (font->width + 7) / 8;
    int per_page = 0;
    for (int line = 0; line < rows; ++line) {
        int left, right;
        if (circle_span(28 + line * (font->height + 4), font->height, &left, &right)) {
            int count = (right - left + 1) / font->width;
            per_page += count < columns ? count : columns;
        }
    }
    int pages = (95 + per_page - 1) / per_page;
    page %= pages;
    int ordinal = 0;
    for (int line = 0; line < rows; ++line) {
        int y = 28 + line * (font->height + 4);
        int left, right;
        if (!circle_span(y, font->height, &left, &right)) {
            continue;
        }
        int count = (right - left + 1) / font->width;
        if (count > columns) {
            count = columns;
        }
        int width = count * font->width;
        int x = (UI_SIZE - width) / 2;
        for (int i = 0; i < width * font->height; ++i) {
            g_am_ui_pixels[i] = BG;
        }
        for (int column = 0; column < count; ++column) {
            int character = 32 + page * per_page + ordinal++;
            if (character > 126) {
                break;
            }
            const uint8_t *glyph = font->get_code_data(font, character);
            if (glyph == NULL) {
                continue;
            }
            for (int row = 0; row < font->height; ++row) {
                for (int bit = 0; bit < font->width; ++bit) {
                    if (glyph[row * bytes_per_row + bit / 8] & (0x80U >> (bit % 8))) {
                        g_am_ui_pixels[row * width + column * font->width + bit] = FG;
                    }
                }
            }
        }
        err = tft_display_draw_bitmap_rgb565(app->display, x, y, width, font->height, g_am_ui_pixels);
        if (err != ESP_OK) {
            return err;
        }
    }
    return ESP_OK;
}

static esp_err_t render_factory(am_app_t *app, const am_model_t *model, int64_t now)
{
    am_test_t test = model->test;
    uint16_t color = model->test_color;
    uint8_t led_index = model->test_led_index;
    rgb_led_color_t led = model->test_led_color;
    uint8_t led_brightness = model->test_led_brightness;
    uint8_t backlight = app->tft_brightness;
    uint8_t font = model->test_font;
    int step = 0;
    if (test == AM_TEST_AUTO) {
        static const uint16_t s_colors[] = {TFT_COLOR_BLACK, TFT_COLOR_WHITE, TFT_COLOR_RED, TFT_COLOR_GREEN,
                                            TFT_COLOR_BLUE, TFT_RGB565(128, 128, 128)};
        static const rgb_led_color_t s_led_colors[] = {RGB_LED_COLOR_RED, RGB_LED_COLOR_GREEN, RGB_LED_COLOR_BLUE, RGB_LED_COLOR_WHITE};
        step = (int)(((now - model->test_started_us) / 2000000) % 34);
        if (step < 6) {
            test = AM_TEST_COLOR;
            color = s_colors[step];
        } else if (step < 10) {
            test = AM_TEST_LED;
            led_index = 16;
            led = s_led_colors[step - 6];
            led_brightness = 96;
        } else if (step < 26) {
            test = AM_TEST_LED;
            led_index = step - 10;
            led = RGB_LED_COLOR_WHITE;
            led_brightness = 96;
        } else if (step < 29) {
            static const uint8_t s_levels[] = {32, 128, 255};
            test = AM_TEST_BACKLIGHT;
            backlight = s_levels[step - 26];
        } else {
            test = AM_TEST_FONT;
            font = step - 29;
        }
    } else if (test == AM_TEST_BACKLIGHT) {
        backlight = model->test_backlight;
    }
    esp_err_t err = tft_display_set_brightness(app->display, backlight);
    if (err != ESP_OK) {
        return err;
    }
    err = rgb_led_strip_set_brightness(app->strip, test == AM_TEST_LED ? led_brightness : app->led_brightness);
    if (err != ESP_OK) {
        return err;
    }
    err = rgb_led_strip_clear(app->strip);
    if (err == ESP_OK && test == AM_TEST_LED) {
        err = led_index == 16 ? rgb_led_strip_fill(app->strip, led) : rgb_led_strip_set_pixel(app->strip, led_index, led);
    }
    if (err == ESP_OK) {
        err = rgb_led_strip_show(app->strip);
    }
    if (err != ESP_OK) {
        return err;
    }
    if (test == AM_TEST_COLOR) {
        return tft_display_fill(app->display, color);
    }
    if (test == AM_TEST_FONT) {
        int page = (int)(now / 2000000);
        return render_font_sample(app, font, page);
    }
    memset(&s_next_frame, 0, sizeof(s_next_frame));
    label(UI_CENTERED_X, 37, "FACTORY TEST", ACCENT);
    label(UI_CENTERED_X, 77, test == AM_TEST_LED ? "LED TEST" : test == AM_TEST_BACKLIGHT ? "BACKLIGHT TEST" : "USB SERIAL READY", FG);
    char value[32];
    if (test == AM_TEST_LED) {
        snprintf(value, sizeof(value), "LED %s RGB %u", led_index == 16 ? "ALL" : "ONE", led_brightness);
        label(UI_CENTERED_X, 108, value, MUTED);
    } else if (test == AM_TEST_BACKLIGHT) {
        snprintf(value, sizeof(value), "LEVEL %u", backlight);
        label(UI_CENTERED_X, 108, value, MUTED);
    } else {
        label(UI_CENTERED_X, 108, "AT+TESTAUTO=1", MUTED);
    }
    label(UI_CENTERED_X, 167, "AT+MODE=NORMAL", WARNING);
    err = tft_display_fill(app->display, BG);
    if (err != ESP_OK) {
        return err;
    }
    for (int i = 0; i < s_next_frame.line_count; ++i) {
        err = draw_line(app, &s_next_frame.lines[i]);
        if (err != ESP_OK) {
            return err;
        }
    }
    return ESP_OK;
}


#define C_FG TFT_RGB565(242, 244, 255)
#define C_MUTED TFT_RGB565(163, 175, 211)
#define C_LINE TFT_RGB565(48, 54, 83)
#define C_MINT TFT_RGB565(54, 226, 154)
#define C_AMBER TFT_RGB565(255, 176, 46)
#define C_CORAL TFT_RGB565(255, 95, 122)
#define C_CODEX TFT_RGB565(139, 153, 255)
#define C_CODEX_TITLE TFT_RGB565(101, 80, 200)
#define C_CLAUDE TFT_RGB565(217, 119, 87)

static struct am_ui_scene s_scene;
static struct am_status_animation s_status_animation;

static void centered(int y, int width, const struct am_font *font, uint16_t color, const char *text)
{
    am_scene_text(&s_scene, (240 - width) / 2, y, width, font, color, text, 0);
}

static void business_header(bool connected)
{
    am_scene_rect(&s_scene, 0, 0, 240, 39, connected ? TFT_RGB565(31, 191, 116) : TFT_RGB565(232, 68, 92), 0);
    am_scene_mask(&s_scene, 107, 8, connected ? &g_am_image_connected : &g_am_image_disconnected, TFT_COLOR_WHITE);
}

static bool local_calendar(const am_model_t *model, int64_t utc, struct tm *out)
{
    if (!model->clock_valid || !model->utc_offset_valid) {
        return false;
    }
    time_t seconds = (time_t)(utc + model->utc_offset_minutes * 60LL);
    return gmtime_r(&seconds, out) && out->tm_year >= 70 && out->tm_year <= 8099;
}

static void calendar_text(const am_model_t *model, int64_t utc, char *date, size_t date_size, char *clock, size_t clock_size, bool seconds)
{
    struct tm tm;
    if (!local_calendar(model, utc, &tm)) {
        snprintf(date, date_size, "---- -- --");
        snprintf(clock, clock_size, "%s", seconds ? "--:--:--" : "--:--");
        return;
    }
    strftime(date, date_size, "%Y-%m-%d", &tm);
    strftime(clock, clock_size, seconds ? "%H:%M:%S" : "%H:%M", &tm);
}

static void connection_page(const am_model_t *model)
{
    const char *label = "等待主机";
    const char *detail = "USB 串口就绪";
    const struct am_image *image = &g_am_image_signal;
    if (model->link == AM_LINK_HANDSHAKE) {
        label = "正在握手";
        detail = "发现设备中";
    } else if (model->link == AM_LINK_SYNCING) {
        label = "正在同步";
        detail = "会话写入中";
    }
    uint16_t sky = TFT_RGB565(86, 204, 242);
    am_scene_mask(&s_scene, 101, 57, image, sky);
    centered(107, 180, &g_am_font_connect, sky, label);
    centered(150, 190, &g_am_font_body, TFT_RGB565(181, 191, 223), detail);
    centered(185, 170, &g_am_font_label, TFT_RGB565(125, 135, 173), "AGENTMATE");
}

static void idle_page(const am_model_t *model, int64_t utc, int64_t now)
{
    char date[20], clock[16];
    calendar_text(model, utc, date, sizeof(date), clock, sizeof(clock), false);
    centered(55, 180, &g_am_font_title, TFT_RGB565(146, 164, 201), "日期/时间");
    bool valid = clock[0] != '-';
    bool colon_visible = !valid || now / 500000 % 2 == 0;
    clock[2] = 0;
    const struct am_font *font = &g_am_font_clock;
    int hour_width = am_text_width(font, clock);
    int colon_width = am_text_width(font, ":");
    int minute_width = am_text_width(font, clock + 3);
    int x = (240 - hour_width - colon_width - minute_width) / 2;
    am_scene_text(&s_scene, x, 86, hour_width, font, C_FG, clock, -1);
    /* The clock font's colon ink center is five pixels below its digit center. */
    am_scene_text(&s_scene, x + hour_width, 81, colon_width, font, colon_visible ? C_FG : AM_UI_BG, ":", -1);
    am_scene_text(&s_scene, x + hour_width + colon_width, 86, minute_width, font, C_FG, clock + 3, -1);
    centered(151, 190, &g_am_font_date, TFT_RGB565(198, 208, 237), date);
}

static void info_page(const am_app_t *app, const am_model_t *model, uint8_t tft, uint8_t led, int64_t utc)
{
    static const char *const s_labels[] = {"设备 ID", "型号", "软件版本", "协议版本", "屏幕亮度", "灯环亮度", "日期", "时间"};
    char date[20], clock[16], brightness[2][8];
    calendar_text(model, utc, date, sizeof(date), clock, sizeof(clock), true);
    snprintf(brightness[0], sizeof(brightness[0]), "%u%%", am_brightness_to_percent(tft));
    snprintf(brightness[1], sizeof(brightness[1]), "%u%%", am_brightness_to_percent(led));
    const char *values[] = {app->device_id + 24, AM_PRODUCT_MODEL, AM_SOFTWARE_VERSION, AM_PROTOCOL_VERSION_TEXT,
                            brightness[0], brightness[1], date, clock};
    for (int i = 0; i < 8; ++i) {
        int y = 47 + i * 19 + (i >= 6 ? 5 : 0);
        am_scene_text(&s_scene, 32, y, 81, &g_am_font_body, TFT_RGB565(159, 172, 210), s_labels[i], 1);
        am_scene_text(&s_scene, 127, y, 85, &g_am_font_info, C_FG, values[i], -1);
    }
}

static void session_page(const am_session_t *session, const struct am_ui_selection *selection, int64_t now, uint32_t generation)
{
    bool claude = strcmp(session->provider, "claude") == 0;
    uint16_t provider_color = claude ? C_CLAUDE : C_CODEX;
    uint16_t title_color = claude ? C_CLAUDE : C_CODEX_TITLE;
    am_scene_bitmap(&s_scene, 27, 84, 36, 36, strcmp(session->provider, "claude") == 0 ? s_am_icon_claude : s_am_icon_codex);
    am_scene_rect(&s_scene, 69, 56, 1, 146, TFT_RGB565(86, 96, 135), 0);
    char identity[32];
    snprintf(identity, sizeof(identity), "%s·%s", session->host_label, session->session_short_name);
    static struct am_font s_identity_fonts[6];
    const struct am_font *identity_font = &s_identity_fonts[0];
    const uint8_t scales[][2] = {{1, 1}, {7, 8}, {3, 4}, {2, 3}, {1, 2}, {2, 5}};
    for (size_t i = 0; i < sizeof(scales) / sizeof(scales[0]); ++i) {
        if (s_identity_fonts[i].glyphs == NULL) {
            s_identity_fonts[i] = g_am_font_identity;
            s_identity_fonts[i].scale_num = scales[i][0];
            s_identity_fonts[i].scale_den = scales[i][1];
        }
        identity_font = &s_identity_fonts[i];
        if (am_text_width(identity_font, identity) <= 121 && am_text_ink_width(identity_font, identity) <= 121) {
            break;
        }
    }
    int bar_width = am_text_width(identity_font, identity) + 16;
    am_scene_rect(&s_scene, 78, 58, bar_width, 24, title_color, 7);
    am_scene_text(&s_scene, 86, 59 + (19 - (identity_font->height * identity_font->scale_num / identity_font->scale_den)) / 2,
                  bar_width - 16, identity_font, TFT_COLOR_WHITE, identity, -1);
    bool large_directory = am_text_width(&g_am_font_directory_large, session->project_name) <= 137 &&
                           am_text_ink_width(&g_am_font_directory_large, session->project_name) <= 137;
    const struct am_font *directory_font = large_directory ?
                                           &g_am_font_directory_large : &g_am_font_directory;
    am_scene_text(&s_scene, 78, 84, 137, directory_font, provider_color, session->project_name, -1);
    const char *label = "工作中";
    const struct am_font *font = &g_am_font_state;
    if (strcmp(session->state, "thinking") == 0) {
        label = "思考中";
    } else if (strcmp(session->state, "waiting_for_input") == 0) {
        label = "等待输入";
    } else if (strcmp(session->state, "permission_required") == 0) {
        label = "需要批准";
    } else if (strcmp(session->state, "stale") == 0) {
        label = "状态未更新";
        font = &g_am_font_state_long;
    } else if (strcmp(session->state, "task_success") == 0) {
        label = "任务完成";
    } else if (strcmp(session->state, "task_failed") == 0) {
        label = "任务失败";
    } else if (strcmp(session->state, "task_canceled") == 0) {
        label = "任务取消";
    }
    struct am_status_visual visual = am_status_select(&s_status_animation, session, generation, now);
    uint16_t color = visual.color;
    am_scene_status(&s_scene, &visual);
    am_scene_text(&s_scene, 88, 113 + (42 - font->height) / 2, 117, font, color, label, 0);
    uint64_t duration = am_task_duration(session, now);
    char minutes[32], seconds[4];
    snprintf(minutes, sizeof(minutes), "%llu", (unsigned long long)(duration / 60));
    if (duration / 60 < 10) {
        snprintf(minutes, sizeof(minutes), "%02u", (unsigned)(duration / 60));
    }
    snprintf(seconds, sizeof(seconds), "%02u", (unsigned)(duration % 60));
    font = &g_am_font_duration;
    int full_width = am_text_width(font, minutes) + am_text_width(font, ":") + am_text_width(font, seconds) + 4;
    if (full_width > 142) {
        font = &g_am_font_duration_long;
    }
    int minute_width = am_text_width(font, minutes);
    int colon_width = am_text_width(font, ":");
    bool advancing = am_state_advancing(session->state);
    uint16_t number_color = advancing ? TFT_RGB565(246, 247, 252) : TFT_RGB565(125, 134, 175);
    am_scene_text(&s_scene, 78, 163, minute_width, font, number_color, minutes, -1);
    am_scene_text(&s_scene, 80 + minute_width, 163, colon_width, font,
                  advancing ? TFT_RGB565(162, 255, 204) : TFT_RGB565(98, 110, 147), ":", -1);
    am_scene_text(&s_scene, 82 + minute_width + colon_width, 163, 55, font, number_color, seconds, -1);
    if (selection->count > 1) {
        for (int i = 0; i < selection->count; ++i) {
            double angle = (90.0 - (i - (selection->count - 1) / 2.0) * 13.0) * 3.14159265 / 180.0;
            am_scene_disc(&s_scene, (int)(120 + cos(angle) * 101), (int)(120 + sin(angle) * 101),
                          i == selection->current ? 3 : 2, i == selection->current ? color : TFT_RGB565(59, 64, 112));
        }
    }
    if (session->context_mode == 2) {
        char context[3][32];
        snprintf(context[0], sizeof(context[0]), "%u.%02u%%", session->ctx_pct_bp / 100, session->ctx_pct_bp % 100);
        snprintf(context[1], sizeof(context[1]), "%lluk", (unsigned long long)(session->ctx_used / 1000));
        snprintf(context[2], sizeof(context[2]), "%lluk", (unsigned long long)(session->ctx_window / 1000));
        for (int i = 0; i < 3; ++i) {
            am_scene_text(&s_scene, 21, 128 + i * 13, 48, &g_am_font_small, C_MUTED, context[i], 0);
        }
    }
}

static void reset_text(const am_model_t *model, const am_usage_t *usage, int64_t utc, char *date, size_t date_capacity,
                       char *clock, size_t clock_capacity)
{
    struct tm current, reset;
    snprintf(date, date_capacity, "未知");
    snprintf(clock, clock_capacity, "未知");
    if (!usage->reset_at || usage->reset_at > 253402300799ULL || !usage->reset_offset_valid || !local_calendar(model, utc, &current)) {
        return;
    }
    time_t local_reset = (time_t)(usage->reset_at + usage->reset_utc_offset_minutes * 60LL);
    if (!gmtime_r(&local_reset, &reset)) {
        return;
    }
    if (current.tm_year != reset.tm_year) {
        snprintf(date, date_capacity, "%d/%d/%d", reset.tm_year + 1900, reset.tm_mon + 1, reset.tm_mday);
    } else {
        snprintf(date, date_capacity, "%d月%d日", reset.tm_mon + 1, reset.tm_mday);
    }
    snprintf(clock, clock_capacity, "%02d:%02d", reset.tm_hour, reset.tm_min);
}

static void usage_page(const am_model_t *model, const struct am_ui_selection *selection, int64_t utc)
{
    am_scene_bitmap(&s_scene, 28, 59, 36, 36, s_am_icon_codex);
    am_scene_rect(&s_scene, 77, 53, 1, 46, C_LINE, 0);
    am_scene_text(&s_scene, 91, 62, 120, &g_am_font_title, C_FG, "剩余用量", -1);
    bool empty = selection->usage[0] < 0 && selection->usage[1] < 0;
    bool single = !empty && (selection->usage[0] < 0 || selection->usage[1] < 0);
    const am_usage_t unknown = {.used_bp = -1};
    int left = single ? 72 : 36;
    int width = single ? 96 : 84;
    if (!single) {
        am_scene_rect(&s_scene, 120, 108, 1, 97, C_LINE, 0);
    }
    for (int i = 0; i < 2; ++i) {
        if (!empty && selection->usage[i] < 0) {
            continue;
        }
        const am_usage_t *usage = empty ? &unknown : &model->usages[selection->usage[i]];
        int x = single ? left : left + i * width;
        am_scene_text(&s_scene, x, 105, width, &g_am_font_title, C_MUTED, i ? "一周" : "5小时", 0);
        char value[20], date[32], clock[16];
        const struct am_font *font = &g_am_font_quota;
        uint16_t value_color = C_MUTED;
        if (usage->used_bp < 0) {
            snprintf(value, sizeof(value), "用量未知");
            font = &g_am_font_date;
        } else {
            int tenths = (10000 - usage->used_bp + 5) / 10;
            snprintf(value, sizeof(value), "%d.%d%%", tenths / 10, tenths % 10);
            /* Grade the rounded value shown on screen, including the threshold boundaries. */
            value_color = tenths >= 500 ? C_MINT : tenths >= 200 ? C_AMBER : C_CORAL;
        }
        am_scene_text(&s_scene, x, 130, width, font, value_color, value, 0);
        reset_text(model, usage, utc, date, sizeof(date), clock, sizeof(clock));
        if (empty) {
            snprintf(date, sizeof(date), "--");
            snprintf(clock, sizeof(clock), "--");
        }
        if (i) {
            const struct am_font *date_font = am_text_width(&g_am_font_body, date) <= width ? &g_am_font_body : &g_am_font_label;
            am_scene_text(&s_scene, x, 165, width, date_font, C_MUTED, date, 0);
        }
        am_scene_text(&s_scene, x, i ? 185 : 176, width, &g_am_font_body, C_MUTED, clock, 0);
    }
}

static void boot_page(void)
{
    am_scene_bitmap(&s_scene, 85, 45, 70, 70, g_am_brand_pixels);
    centered(118, 180, &g_am_font_connect, C_FG, "AgentMate");
    centered(155, 180, &g_am_font_label, TFT_RGB565(145, 160, 207), AM_PRODUCT_MODEL);
    centered(184, 140, &g_am_font_body, TFT_RGB565(194, 202, 232), AM_SOFTWARE_VERSION);
}

static esp_err_t animate_led(am_app_t *app, struct am_led_state *state, const struct am_led_effect *effect, int64_t now,
                             rgb_led_color_t colors[16], bool force)
{
    static rgb_led_color_t s_previous[16];
    am_led_frame(state, effect, now, colors);
    if (!force && memcmp(colors, s_previous, sizeof(s_previous)) == 0) {
        return ESP_OK;
    }
    esp_err_t err = rgb_led_strip_set_pixels(app->strip, 0, colors, 16);
    if (err == ESP_OK) {
        err = rgb_led_strip_show(app->strip);
    }
    if (err == ESP_OK) {
        memcpy(s_previous, colors, sizeof(s_previous));
    }
    return err;
}

void am_ui_run(am_app_t *app)
{
    /* Prepare geometry before starting Boot so first session entry has no cache stall. */
    am_status_prepare();
    static am_model_t s_snapshot;
    static struct am_ui_scene s_previous;
    static struct am_ui_schedule s_schedule;
    uint64_t factory_key = UINT64_MAX;
    uint16_t rotation_before = UINT16_MAX;
    enum am_page page_before = AM_PAGE_BOOT;
    char key_before[9] = "";
    int64_t boot_started = esp_timer_get_time();
    int64_t transition_started = 0;
    bool boot_done = false;
    bool connect_first = false;
    bool info_before = false;
    struct am_led_effect retained = {0};
    static struct am_led_state s_led_state;
    for (;;) {
        int64_t now = esp_timer_get_time();
        int64_t utc = (int64_t)time(NULL);
        xSemaphoreTake(app->model_lock, portMAX_DELAY);
        memcpy(&s_snapshot, &app->model, sizeof(s_snapshot));
        uint16_t rotation = app->rotation;
        uint8_t tft = app->tft_brightness;
        uint8_t led = app->led_brightness;
        xSemaphoreGive(app->model_lock);
        bool full = rotation != rotation_before;
        int64_t age = now - boot_started;
        if (!boot_done && age >= 2000000 && (s_snapshot.protocol_ready || age >= 5000000)) {
            boot_done = true;
            connect_first = !s_snapshot.factory_mode;
            full = true;
        }
        memset(&s_scene, 0, sizeof(s_scene));
        s_scene.body_alpha = 255;
        if (!boot_done) {
            boot_page();
            s_scene.body_alpha = age < 300000 ? (uint8_t)((age / 80000) * 80 * 255 / 300) : 255;
            xSemaphoreTake(app->display_lock, portMAX_DELAY);
            esp_err_t err = am_scene_render(app, &s_previous, &s_scene, full);
            if (err == ESP_OK) {
                rotation_before = rotation;
            }
            (void)rgb_led_strip_clear(app->strip);
            (void)rgb_led_strip_show(app->strip);
            xSemaphoreGive(app->display_lock);
            vTaskDelay(pdMS_TO_TICKS(80));
            continue;
        }
        if (s_snapshot.factory_mode) {
            uint32_t stage = s_snapshot.test == AM_TEST_AUTO ? (uint32_t)((now - s_snapshot.test_started_us) / 2000000) :
                             s_snapshot.test == AM_TEST_FONT ? (uint32_t)(now / 2000000) : 0;
            uint64_t key = ((uint64_t)s_snapshot.revision << 32) | stage;
            if (key != factory_key || full) {
                xSemaphoreTake(app->display_lock, portMAX_DELAY);
                esp_err_t err = render_factory(app, &s_snapshot, now);
                xSemaphoreGive(app->display_lock);
                if (err == ESP_OK) {
                    factory_key = key;
                    rotation_before = rotation;
                }
            }
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }
        struct am_ui_selection selection;
        am_ui_select(&s_snapshot, &s_schedule, now, &selection);
        if (connect_first) {
            selection.page = AM_PAGE_CONNECT;
            connect_first = false;
        }
        bool info = selection.page == AM_PAGE_INFO;
        struct am_led_effect effect = am_led_select(&s_snapshot, &selection);
        if (info && !info_before) {
            retained = effect;
        }
        if (info && s_snapshot.link == AM_LINK_ONLINE) {
            effect = retained;
        }
        const char *session_key = selection.count ? s_snapshot.sessions[selection.slots[selection.current]].key : "";
        bool page_changed = page_before != selection.page ||
                            ((selection.page == AM_PAGE_TASK || selection.page == AM_PAGE_RESULT) && strcmp(key_before, session_key) != 0);
        if (page_changed) {
            transition_started = selection.preempted || page_before == AM_PAGE_BOOT ? 0 : now;
        }
        if (transition_started && now - transition_started < 200000) {
            s_scene.body_alpha = (uint8_t)(((now - transition_started) / 100000) * 128);
        }
        business_header(s_snapshot.link == AM_LINK_ONLINE && selection.page != AM_PAGE_CONNECT);
        switch (selection.page) {
        case AM_PAGE_CONNECT:
            connection_page(&s_snapshot);
            break;
        case AM_PAGE_TASK:
        case AM_PAGE_RESULT:
            session_page(&s_snapshot.sessions[selection.slots[selection.current]], &selection, now, s_snapshot.remote_generation);
            break;
        case AM_PAGE_USAGE:
            usage_page(&s_snapshot, &selection, utc);
            break;
        case AM_PAGE_INFO:
            info_page(app, &s_snapshot, tft, led, utc);
            break;
        default:
            idle_page(&s_snapshot, utc, now);
            break;
        }
        rgb_led_color_t led_colors[16];
        int64_t render_started = esp_timer_get_time();
        xSemaphoreTake(app->display_lock, portMAX_DELAY);
        esp_err_t err = am_scene_render(app, &s_previous, &s_scene, full);
        esp_err_t led_err = animate_led(app, &s_led_state, &effect, now, led_colors, full);
        xSemaphoreGive(app->display_lock);
        if (err == ESP_OK) {
            rotation_before = rotation;
            page_before = selection.page;
            snprintf(key_before, sizeof(key_before), "%s", session_key);
        }
        xSemaphoreTake(app->model_lock, portMAX_DELAY);
        uint8_t slot = selection.page == AM_PAGE_TASK || selection.page == AM_PAGE_RESULT ?
                       (uint8_t)(selection.slots[selection.current] + 1) : 0;
        if (err == ESP_OK && (app->ui_page_sequence == 0 || app->ui_page != (uint8_t)selection.page || app->ui_slot != slot)) {
            app->ui_page = (uint8_t)selection.page;
            app->ui_slot = slot;
            app->ui_page_sequence++;
        }
        if (err == ESP_OK) {
            app->ui_group = (uint8_t)selection.group;
            uint8_t status_kind = UINT8_MAX, status_frame = UINT8_MAX;
            for (int i = 0; i < s_scene.count; ++i) {
                if (s_scene.ops[i].kind == AM_OP_STATUS_FRAME) {
                    status_kind = (uint8_t)s_scene.ops[i].status_kind;
                    status_frame = s_scene.ops[i].animation_frame;
                    break;
                }
            }
            if (app->ui_status_sequence == 0 || app->ui_status_kind != status_kind || app->ui_status_frame != status_frame) {
                app->ui_status_kind = status_kind;
                app->ui_status_frame = status_frame;
                app->ui_status_sequence++;
            }
        }
        if (led_err == ESP_OK) {
            app->ui_led_mode = (uint8_t)effect.mode;
            app->ui_led_rgb = ((uint32_t)effect.color.r << 16) | ((uint32_t)effect.color.g << 8) | effect.color.b;
            app->ui_led_mask = 0;
            app->ui_led_sum = 0;
            app->ui_led_pixel0 = ((uint32_t)led_colors[0].r << 16) | ((uint32_t)led_colors[0].g << 8) | led_colors[0].b;
            app->ui_led_pixel8 = ((uint32_t)led_colors[8].r << 16) | ((uint32_t)led_colors[8].g << 8) | led_colors[8].b;
            for (int i = 0; i < 16; ++i) {
                app->ui_led_sum += led_colors[i].r + led_colors[i].g + led_colors[i].b;
                if (led_colors[i].r || led_colors[i].g || led_colors[i].b) {
                    app->ui_led_mask |= (uint16_t)(1U << i);
                }
            }
        }
        uint32_t render_us = (uint32_t)(esp_timer_get_time() - render_started);
        if (render_us > app->ui_render_max_us) {
            app->ui_render_max_us = render_us;
        }
        app->ui_stack_free = uxTaskGetStackHighWaterMark(NULL);
        xSemaphoreGive(app->model_lock);
        info_before = info;
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}
