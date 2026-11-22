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
# 2026-10-25 5.0      Codex         Display host-selected Bluetooth batteries in adaptive standby layouts.
# 2026-10-25 4.9      Codex         Count active sessions plus retained results independently of rotation priority.
# 2026-10-25 4.8      Codex         Replace session dots with static glowing selection arcs.
# 2026-10-24 4.7      Codex         Render indicators exclusively from the current rotation selection.
# 2026-10-24 4.6      Codex         Exclude inactive sessions from indicator counts and positions.
# 2026-10-24 4.5      Codex         Center session indicators within fixed left and right arc positions.
# 2026-10-24 4.4      Codex         Render custom RGB standby clock palettes.
# 2026-10-24 4.3      Codex         Use five fixed left dot positions and three fixed right positions.
# 2026-10-24 4.2      Codex         Show state-colored session dots on the lower-left arc with a breathing selection.
# 2026-10-13 4.1      Codex         Let clock previews bypass the initial connection page.
# 2026-10-12 4.0      Codex         Display exclusive OTA progress and publish boot health.
# 2026-10-12 3.9      Codex         Default idle clock to amber with selectable persistent palettes.
# 2026-10-12 3.8      Codex         Implement v6.2 black pages, curved headers and standby panels.
# 2026-10-11 3.7      Codex         Restore rounded header time and provider-colored directories; skip invalid metrics.
# 2026-10-11 3.6      Codex         Display ASCII directories and restore black status panels.
# 2026-10-11 3.5      Codex         Implement five-row Session layout and rotating detail panels.
# 2026-10-06 3.3      Codex         Lower five-hour quota and enlarge both quota numbers.
# 2026-10-06 3.2      Codex         Show Codex remaining quotas as two session-page progress bars.
# 2026-10-05 3.1      Codex         Use a middle dot between host and session names.
# 2026-10-04 3.0      Codex         Add protocol 2 host labels and session display names.
#
# Date       Version  Author       Description
# ---------- -------- ------------ -----------------------------------------------
# 2026-10-09 3.4      Codex         Alternate Codex quota and context below the provider icon.
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

#include "app.h"

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
#include "ui_state.h"
#include "led.h"
#include "ui_render.h"
#include "text.h"

#define FG TFT_RGB565(234, 240, 249)
#define UI_SIZE 240
#define UI_CENTER_TWICE (UI_SIZE - 1)
#define UI_SAFE_RADIUS_TWICE 232

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

static esp_err_t render_factory_page(am_app_t *app, am_test_t test, uint8_t led_index, rgb_led_color_t led,
                                      uint8_t brightness, uint8_t backlight, uint8_t font_index);

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
    esp_err_t err = render_factory_page(app, AM_TEST_FONT, 0, (rgb_led_color_t){0}, 0, 0, index);
    if (err != ESP_OK) {
        return err;
    }
    int left, right;
    if (!circle_span(98, font->height, &left, &right)) {
        return ESP_OK;
    }
    int count = (right - left + 1 < 184 ? right - left + 1 : 184) / font->width;
    int pages = 1 + (95 + count - 1) / count;
    page %= pages;
    static const char s_sample[] = "ABC 0123 45.67%";
    int width = count * font->width;
    memset(g_am_ui_pixels, 0, (size_t)width * font->height * sizeof(*g_am_ui_pixels));
    for (int column = 0; column < count; ++column) {
        int ch = page ? 32 + (page - 1) * count + column : column < sizeof(s_sample) - 1 ? s_sample[column] : ' ';
        if (ch > 126) {
            break;
        }
        const uint8_t *glyph = font->get_code_data(font, ch);
        if (!glyph) {
            continue;
        }
        for (int row = 0; row < font->height; ++row) {
            for (int bit = 0; bit < font->width; ++bit) {
                if (glyph[row * ((font->width + 7) / 8) + bit / 8] & (0x80U >> (bit % 8))) {
                    g_am_ui_pixels[row * width + column * font->width + bit] = FG;
                }
            }
        }
    }
    return tft_display_draw_bitmap_rgb565(app->display, (240 - width) / 2, 98, width, font->height, g_am_ui_pixels);
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
    return render_factory_page(app, test, led_index, led, led_brightness, backlight, font);
}


#define C_FG TFT_RGB565(242, 244, 255)
#define C_MUTED TFT_RGB565(163, 175, 211)
#define C_MINT TFT_RGB565(86, 214, 161)
#define C_AMBER TFT_RGB565(255, 195, 106)
#define C_CORAL TFT_RGB565(255, 143, 166)
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
    am_scene_cap(&s_scene, 39, 96, 39, connected ? TFT_RGB565(31, 191, 116) : TFT_RGB565(232, 68, 92), 0);
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
        image = &g_am_image_link;
    } else if (model->link == AM_LINK_SYNCING) {
        label = "正在同步";
        detail = "会话写入中";
        image = &g_am_image_sync;
    }
    uint16_t sky = TFT_RGB565(86, 204, 242);
    am_scene_mask(&s_scene, 101, 57, image, sky);
    centered(107, 180, &g_am_font_connect, sky, label);
    centered(150, 190, &g_am_font_body, TFT_RGB565(181, 191, 223), detail);
    centered(185, 170, &g_am_font_label, TFT_RGB565(125, 135, 173), "AGENTMATE · USB");
}

struct idle_palette {
    uint16_t time, date, year, label, rule;
};

static const struct idle_palette s_idle_palettes[AM_CLOCK_THEME_COUNT] = {
    [AM_CLOCK_AMBER] = {TFT_RGB565(241, 188, 116), TFT_RGB565(211, 223, 221), TFT_RGB565(147, 133, 115),
                        TFT_RGB565(163, 151, 132), TFT_RGB565(94, 72, 48)},
    [AM_CLOCK_WHITE] = {TFT_RGB565(246, 245, 255), TFT_RGB565(226, 232, 246), TFT_RGB565(146, 164, 201),
                        TFT_RGB565(146, 164, 201), TFT_RGB565(53, 64, 83)},
    [AM_CLOCK_MINT] = {TFT_RGB565(119, 230, 188), TFT_RGB565(206, 235, 221), TFT_RGB565(109, 157, 139),
                       TFT_RGB565(125, 156, 143), TFT_RGB565(39, 79, 64)},
    [AM_CLOCK_CODEX] = {TFT_RGB565(182, 160, 242), TFT_RGB565(211, 200, 234), TFT_RGB565(137, 117, 180),
                        TFT_RGB565(155, 139, 181), TFT_RGB565(69, 53, 97)},
};

static void idle_page(const am_model_t *model, int64_t utc, int64_t now)
{
    unsigned theme = model->clock_theme < AM_CLOCK_THEME_COUNT ? model->clock_theme : AM_CLOCK_AMBER;
    struct idle_palette custom;
    const struct idle_palette *palette = &s_idle_palettes[theme];
    if (theme == AM_CLOCK_CUSTOM) {
        unsigned r = (model->clock_rgb >> 16) & 255;
        unsigned g = (model->clock_rgb >> 8) & 255;
        unsigned b = model->clock_rgb & 255;
        custom = (struct idle_palette){
            TFT_RGB565(r, g, b),
            TFT_RGB565((r * 65 + 255 * 35) / 100, (g * 65 + 255 * 35) / 100, (b * 65 + 255 * 35) / 100),
            TFT_RGB565(r * 60 / 100, g * 60 / 100, b * 60 / 100),
            TFT_RGB565(r * 65 / 100, g * 65 / 100, b * 65 / 100),
            TFT_RGB565(r * 30 / 100, g * 30 / 100, b * 30 / 100)
        };
        palette = &custom;
    }
    char date[20], clock[16];
    calendar_text(model, utc, date, sizeof(date), clock, sizeof(clock), false);
    centered(54, 180, &g_am_font_body, palette->label, "日期/时间");
    bool valid = clock[0] != '-';
    bool colon_visible = !valid || now / 500000 % 2 == 0;
    uint16_t color = valid ? palette->time : TFT_RGB565(141, 152, 183);
    clock[2] = 0;
    const struct am_font *font = &g_am_font_standby_clock;
    int hour_width = am_text_width(font, clock);
    int colon_width = am_text_width(font, ":");
    int minute_width = am_text_width(font, clock + 3);
    int x = (240 - hour_width - colon_width - minute_width) / 2;
    am_scene_text(&s_scene, x, 87, hour_width, font, color, clock, -1);
    am_scene_text(&s_scene, x + hour_width, 87, colon_width, font, colon_visible ? color : AM_UI_BG, ":", -1);
    am_scene_text(&s_scene, x + hour_width + colon_width, 87, minute_width, font, color, clock + 3, -1);
    am_scene_rect(&s_scene, 83, 147, 74, 1, palette->rule, 0);
    char year[5] = "----";
    if (valid) {
        memcpy(year, date, 4);
    }
    centered(151, 180, &g_am_font_standby_year, valid ? palette->year : color, year);
    centered(179, 176, &g_am_font_standby_date, valid ? palette->date : color, valid ? date + 5 : "-- --");
}

static uint16_t battery_color(const struct am_battery *entry)
{
    if (!entry->connected || entry->percent < 0) {
        return TFT_RGB565(137, 148, 157);
    }
    return entry->percent < 30 ? TFT_RGB565(255, 95, 122) :
           entry->percent < 60 ? TFT_RGB565(255, 176, 46) : TFT_RGB565(54, 226, 154);
}

static uint16_t battery_dim(uint16_t color, unsigned percent)
{
    unsigned r = ((color >> 11) & 31) * percent / 100;
    unsigned g = ((color >> 5) & 63) * percent / 100;
    unsigned b = (color & 31) * percent / 100;
    return (uint16_t)((r << 11) | (g << 5) | b);
}

static void battery_name(const struct am_battery *entry, char name[AM_BATTERY_NAME_BYTES + 1], int index)
{
    for (const unsigned char *p = (const unsigned char *)entry->name; *p; ++p) {
        if (*p >= 128) {
            snprintf(name, AM_BATTERY_NAME_BYTES + 1, "%s %d", entry->mouse ? "鼠标" : "键盘", index + 1);
            return;
        }
    }
    snprintf(name, AM_BATTERY_NAME_BYTES + 1, "%s", entry->name);
}

static void battery_reading(const struct am_battery *entry, char value[8])
{
    if (!entry->connected || entry->percent < 0) {
        snprintf(value, 8, "--");
    } else {
        snprintf(value, 8, "%d%%", entry->percent);
    }
}

static void battery_bar(const struct am_battery *entry, int x, int y, int width, uint16_t color)
{
    am_scene_progress(&s_scene, x, y, width, 3, color, battery_dim(color, 18),
                      entry->connected && entry->percent >= 0 ? (unsigned)entry->percent * 100 : 0);
}

static void battery_page(const am_model_t *model, int64_t utc)
{
    s_scene.session_background = true;
    uint16_t green = TFT_RGB565(54, 184, 144);
    am_scene_cap(&s_scene, 76, 40, 40, green, 1);
    char date[20], clock[16], footer[24];
    calendar_text(model, utc, date, sizeof(date), clock, sizeof(clock), false);
    centered(16, 160, &g_am_font_session_clock_rounded, TFT_COLOR_WHITE, clock);
    am_scene_mask(&s_scene, 65, 49, &g_am_image_bluetooth, green);
    am_scene_text(&s_scene, 87, 48, 100, &g_am_font_body, TFT_COLOR_WHITE, "蓝牙电量", 0);
    unsigned count = model->battery_count > AM_MAX_BATTERIES ? AM_MAX_BATTERIES : model->battery_count;
    snprintf(footer, sizeof(footer), "已选 %u 台", count);
    centered(220, 100, &g_am_font_battery_name, TFT_RGB565(131, 151, 143), footer);
    for (unsigned i = 0; i < count; ++i) {
        const struct am_battery *entry = &model->batteries[i];
        uint16_t color = battery_color(entry);
        char name[AM_BATTERY_NAME_BYTES + 1], value[8];
        battery_name(entry, name, i);
        battery_reading(entry, value);
        const struct am_image *icon = entry->mouse ? &g_am_image_battery_mouse : &g_am_image_keyboard;
        if (count == 1) {
            am_scene_rect(&s_scene, 79, 96, 79, 23, color, 5);
            am_scene_rect(&s_scene, 81, 98, 75, 19, TFT_COLOR_BLACK, 3);
            am_scene_rect(&s_scene, 159, 102, 4, 11, color, 1);
            am_scene_progress(&s_scene, 83, 100, 71, 15, color, battery_dim(color, 18),
                              entry->connected && entry->percent >= 0 ? (unsigned)entry->percent * 100 : 0);
            am_scene_text(&s_scene, 40, 131, 160, &g_am_font_battery_large, color, value, 0);
            am_scene_mask(&s_scene, 65, 185, icon, color);
            am_scene_text(&s_scene, 94, 189, 91, &g_am_font_small, TFT_COLOR_WHITE, name, -1);
        } else if (count == 2) {
            int x = 39 + i * 86;
            am_scene_rect(&s_scene, x, 92, 76, 107, battery_dim(color, 9), 10);
            am_scene_mask(&s_scene, x + 27, 103, icon, color);
            am_scene_text(&s_scene, x + 4, 135, 68, &g_am_font_battery_name, TFT_COLOR_WHITE, name, 0);
            const struct am_font *font = am_text_width(&g_am_font_battery_percent, value) > 68 ?
                                         &g_am_font_battery_compact : &g_am_font_battery_percent;
            am_scene_text(&s_scene, x + 4, 153, 68, font, color, value, 0);
            battery_bar(entry, x + 12, 186, 52, color);
        } else if (count == 3) {
            int y = 87 + i * 41;
            am_scene_rect(&s_scene, 47, y, 146, 37, battery_dim(color, 9), 9);
            am_scene_mask(&s_scene, 55, y + 7, icon, color);
            am_scene_text(&s_scene, 85, y + 5, 43, &g_am_font_battery_name, TFT_COLOR_WHITE, name, -1);
            am_scene_text(&s_scene, 130, y + 3, 58, &g_am_font_battery_row, color, value, 1);
            battery_bar(entry, 85, y + 28, 99, color);
        } else {
            int x = 44 + i % 2 * 80, y = 85 + i / 2 * 66;
            am_scene_rect(&s_scene, x, y, 72, 59, battery_dim(color, 9), 8);
            const struct am_image *small = entry->mouse ? &g_am_image_mouse_small : &g_am_image_keyboard_small;
            am_scene_mask(&s_scene, x + 4, y + 4, small, color);
            am_scene_text(&s_scene, x + 20, y + 4, 49, &g_am_font_battery_name, TFT_COLOR_WHITE, name, -1);
            am_scene_text(&s_scene, x + 4, y + 19, 64, &g_am_font_battery_compact, color, value, 0);
            if (!entry->connected || entry->percent < 0) {
                am_scene_text(&s_scene, x + 4, y + 45, 64, &g_am_font_battery_name, color,
                              entry->connected ? "未知" : "已断开", 0);
            } else {
                battery_bar(entry, x + 9, y + 51, 54, color);
            }
        }
    }
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

#define S_CAP TFT_COLOR_BLACK
#define S_QUOTA_TOP TFT_RGB565(69, 50, 84)
#define S_QUOTA_BOTTOM TFT_RGB565(44, 34, 59)
#define S_QUOTA_LABEL TFT_RGB565(222, 205, 235)
#define S_CONTEXT_TOP TFT_RGB565(34, 70, 87)
#define S_CONTEXT_BOTTOM TFT_RGB565(21, 46, 61)
#define S_CONTEXT_LABEL TFT_RGB565(193, 217, 231)
#define S_GREEN TFT_RGB565(86, 214, 161)
#define S_AMBER TFT_RGB565(255, 195, 106)
#define S_CORAL TFT_RGB565(255, 143, 166)
#define S_UNKNOWN TFT_RGB565(142, 161, 151)

static void session_percent(int x, const char *title, const char *value, uint16_t color, bool context)
{
    am_scene_panel(&s_scene, x, 148, 67, 42, 15, context ? S_CONTEXT_TOP : S_QUOTA_TOP, context ? S_CONTEXT_BOTTOM : S_QUOTA_BOTTOM);
    am_scene_text(&s_scene, x + 3, 148, 61, &g_am_font_session_tag, context ? S_CONTEXT_LABEL : S_QUOTA_LABEL, title, 0);
    am_scene_text(&s_scene, x + 3, 163, 61, &g_am_font_session_percent, color, value, 0);
}

static int session_quota_count(const am_model_t *model, const struct am_ui_selection *selection)
{
    int count = 0;
    for (int i = 0; i < 2; ++i) {
        int slot = selection->usage[i];
        if (slot >= 0 && slot < AM_MAX_USAGE_ENTRIES && am_session_quota_available(&model->usages[slot])) {
            count++;
        }
    }
    return count;
}

static void session_quota(const am_model_t *model, const struct am_ui_selection *selection)
{
    int count = session_quota_count(model, selection);
    int x = count == 1 ? 113 : 77;
    for (int i = 0; i < 2; ++i) {
        int slot = selection->usage[i];
        if (slot < 0 || slot >= AM_MAX_USAGE_ENTRIES || !am_session_quota_available(&model->usages[slot])) {
            continue;
        }
        int used_bp = model->usages[slot].used_bp;
        char value[8] = "--";
        uint16_t color = S_UNKNOWN;
        if (used_bp >= 0 && used_bp <= 10000) {
            int percent = (10000 - used_bp + 50) / 100;
            snprintf(value, sizeof(value), "%d%%", percent);
            color = percent >= 50 ? S_GREEN : percent >= 20 ? S_AMBER : S_CORAL;
        }
        session_percent(x, i ? "WEEK" : "5H", value, color, false);
        x += 72;
    }
}

static void session_tokens(uint64_t count, char *text, size_t capacity)
{
    if (count >= 999500000) {
        /* Bound even uint64 protocol values to three digit positions without wrapping. */
        snprintf(text, capacity, ">999M");
    } else if (count >= 999500) {
        uint64_t quantum = count < 10000000 ? 10000 : count < 100000000 ? 100000 : 1000000;
        uint64_t rounded = ((count + quantum / 2) / quantum) * quantum;
        unsigned whole = (unsigned)(rounded / 1000000);
        if (whole < 10) {
            snprintf(text, capacity, "%u.%02uM", whole, (unsigned)(rounded % 1000000 / 10000));
        } else if (whole < 100) {
            snprintf(text, capacity, "%u.%uM", whole, (unsigned)(rounded % 1000000 / 100000));
        } else {
            snprintf(text, capacity, "%uM", whole);
        }
    } else if (count >= 1000) {
        snprintf(text, capacity, "%uK", (unsigned)((count + 500) / 1000));
    } else {
        snprintf(text, capacity, "%u", (unsigned)count);
    }
}

static void session_context(const am_session_t *session)
{
    char percent[8] = "--", used[16] = "--", total[16] = "--";
    uint16_t color = S_UNKNOWN;
    if (session->context_mode == 2) {
        unsigned value = (session->ctx_pct_bp + 50) / 100;
        snprintf(percent, sizeof(percent), "%u%%", value);
        color = value < 50 ? S_GREEN : value < 80 ? S_AMBER : S_CORAL;
        session_tokens(session->ctx_used, used, sizeof(used));
        session_tokens(session->ctx_window, total, sizeof(total));
    }
    session_percent(78, "CTX", percent, color, true);
    am_scene_panel(&s_scene, 151, 148, 64, 42, 21, S_CONTEXT_TOP, S_CONTEXT_BOTTOM);
    am_scene_text(&s_scene, 156, 148, 54, &g_am_font_session_tokens, TFT_RGB565(232, 243, 250), used, 1);
    am_scene_text(&s_scene, 156, 169, 54, &g_am_font_session_tokens, TFT_RGB565(170, 198, 215), total, 1);
}

static void session_duration(const am_session_t *session, int64_t now)
{
    uint64_t seconds = am_task_duration(session, now);
    char value[32];
    snprintf(value, sizeof(value), "%02llu:%02u", (unsigned long long)(seconds / 60), (unsigned)(seconds % 60));
    const struct am_font *font = seconds / 60 < 1000 ? &g_am_font_session_duration : &g_am_font_session_duration_long;
    static struct am_font s_fitted_duration;
    if (am_text_width(font, value) > 145) {
        s_fitted_duration = *font;
        s_fitted_duration.scale_den = font->height;
        do {
            s_fitted_duration.height--;
            s_fitted_duration.scale_num = s_fitted_duration.height;
        } while (am_text_width(&s_fitted_duration, value) > 145 && s_fitted_duration.height > 1);
        font = &s_fitted_duration;
    }
    uint16_t color = am_state_advancing(session->state) ? TFT_RGB565(224, 234, 229) : TFT_RGB565(141, 141, 141);
    am_scene_text(&s_scene, 77, 148 + (42 - font->height) / 2, 145, font, color, value, 0);
}

static void session_directory_text(const char *project, char display[65])
{
    size_t length = 0;
    while (*project && length < 64) {
        uint32_t code = am_utf8_next(&project);
        display[length++] = code >= 32 && code <= 126 ? (char)code : '?';
    }
    display[length] = 0;
}

static void session_indicators(const am_model_t *model, const struct am_ui_selection *selection, int64_t now)
{
    int slots[AM_MAX_SESSIONS];
    int count = 0;
    for (int i = 0; i < AM_MAX_SESSIONS; ++i) {
        if (am_session_displayable(&model->sessions[i])) {
            slots[count++] = i;
        }
    }
    if (count == 0) {
        return;
    }
    /* Center occupied fixed positions; an even count favors the adjacent upper position. */
    int left_count = count < 5 ? count : 5;
    int left_start = (5 - left_count) / 2;
    int right_start = (3 - (count - left_count)) / 2;
    int current_slot = selection->count ? selection->slots[selection->current] : -1;
    for (int index = 0; index < count; ++index) {
        const am_session_t *entry = &model->sessions[slots[index]];
        int position = index < left_count ? left_start + index : 5 + right_start + index - left_count;
        struct am_status_animation animation = {0};
        struct am_status_visual visual = am_status_select(&animation, entry, model->remote_generation, now);
        am_scene_session_arc(&s_scene, position, visual.color, slots[index] == current_slot);
    }
}

static void session_page(const am_model_t *model, const am_session_t *session, const struct am_ui_selection *selection,
                         int64_t now, uint32_t generation, int64_t utc)
{
    s_scene.session_background = true;
    bool claude = strcmp(session->provider, "claude") == 0;
    uint16_t provider_color = claude ? C_CLAUDE : C_CODEX_TITLE;
    am_scene_cap(&s_scene, 76, 40, 40, provider_color, 2);
    char date[20], clock[16];
    calendar_text(model, utc, date, sizeof(date), clock, sizeof(clock), false);
    bool colon_visible = clock[0] == '-' || now / 500000 % 2 == 0;
    const struct am_font *clock_font = &g_am_font_session_clock_rounded;
    int clock_width = am_text_width(clock_font, clock);
    clock[2] = 0;
    int hour_width = am_text_width(clock_font, clock);
    int colon_width = am_text_width(clock_font, ":");
    int minute_width = am_text_width(clock_font, clock + 3);
    int x = (240 - clock_width) / 2;
    am_scene_text(&s_scene, x, 16, hour_width, clock_font, TFT_COLOR_WHITE, clock, -1);
    am_scene_text(&s_scene, x + hour_width, 16, colon_width, clock_font, colon_visible ? TFT_COLOR_WHITE : S_CAP, ":", -1);
    am_scene_text(&s_scene, x + hour_width + colon_width, 16, minute_width, clock_font, TFT_COLOR_WHITE, clock + 3, -1);
    char directory[65];
    session_directory_text(session->project_name, directory);
    am_scene_text(&s_scene, 30, 48, 180, &g_am_font_session_directory, provider_color, directory, 0);
    am_scene_bitmap(&s_scene, 17, 117, 38, 38, claude ? s_am_icon_claude_session : s_am_icon_codex_session);
    am_scene_rect(&s_scene, 63, 96, 1, 80, TFT_RGB565(53, 68, 63), 0);
    static const char *const s_labels[] = {"工作中", "思考中", "等待输入", "需要批准", "状态未更新", "任务完成", "任务失败", "任务取消"};
    struct am_status_visual visual = am_status_select(&s_status_animation, session, generation, now);
    int state = visual.kind;
    const struct am_font *font = state <= AM_STATUS_THINKING ? &g_am_font_session_state :
                                state == AM_STATUS_STALE ? &g_am_font_session_state_long : &g_am_font_session_state_mid;
    am_scene_status(&s_scene, &visual);
    am_scene_text(&s_scene, 84, 86 + (56 - font->height) / 2, 131, font, visual.color, s_labels[state], 0);
    session_indicators(model, selection, now);
    char identity[32];
    snprintf(identity, sizeof(identity), "%s·%s", session->host_label, session->session_short_name);
    int pill_width = am_text_width(&g_am_font_session_identity, identity) + 16;
    pill_width = pill_width > 88 ? 88 : pill_width;
    am_scene_rect(&s_scene, (240 - pill_width) / 2, 208, pill_width, 18,
                  claude ? TFT_RGB565(224, 122, 95) : TFT_RGB565(108, 92, 231), 9);
    am_scene_text(&s_scene, (240 - pill_width) / 2 + 8, 209, pill_width - 16, &g_am_font_session_identity, TFT_COLOR_WHITE, identity, 0);
    if (selection->detail == AM_DETAIL_CONTEXT && am_session_context_available(session)) {
        session_context(session);
    } else if (selection->detail == AM_DETAIL_QUOTA && !claude && session_quota_count(model, selection) > 0) {
        session_quota(model, selection);
    } else {
        session_duration(session, now);
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
    int heading_width = am_text_width(&g_am_font_standby_heading, "剩余用量");
    int heading_x = (240 - 28 - 10 - heading_width) / 2;
    am_scene_bitmap(&s_scene, heading_x, 54, 28, 28, s_am_icon_codex_usage);
    am_scene_text(&s_scene, heading_x + 38, 57, heading_width, &g_am_font_standby_heading, C_FG, "剩余用量", -1);
    am_scene_rect(&s_scene, 28, 89, 184, 1, C_CODEX_TITLE, 0);
    bool empty = selection->usage[0] < 0 && selection->usage[1] < 0;
    bool single = !empty && (selection->usage[0] < 0 || selection->usage[1] < 0);
    const am_usage_t unknown = {.used_bp = -1};
    for (int i = 0; i < 2; ++i) {
        if (!empty && selection->usage[i] < 0) {
            continue;
        }
        const am_usage_t *usage = empty ? &unknown : &model->usages[selection->usage[i]];
        int x = single ? 76 : 28 + i * 96;
        am_scene_panel(&s_scene, x, 95, 88, 110, 24, S_QUOTA_TOP, S_QUOTA_BOTTOM);
        s_scene.ops[s_scene.count - 1].radius = 14;
        am_scene_text(&s_scene, x + 4, 96, 80, &g_am_font_usage_label, S_QUOTA_LABEL, i ? "一周" : "5小时", 0);
        char value[20], date[32], clock[16];
        const struct am_font *font = &g_am_font_quota;
        uint16_t value_color = C_MUTED;
        int tenths = 0;
        if (usage->used_bp < 0 || usage->used_bp > 10000) {
            snprintf(value, sizeof(value), "用量未知");
            font = &g_am_font_date;
        } else {
            tenths = (10000 - usage->used_bp + 5) / 10;
            snprintf(value, sizeof(value), "%d.%d%%", tenths / 10, tenths % 10);
            value_color = tenths >= 500 ? C_MINT : tenths >= 200 ? C_AMBER : C_CORAL;
            if (tenths == 1000) {
                font = &g_am_font_quota_compact;
            }
        }
        am_scene_text(&s_scene, x + 4, 119 + (40 - font->height) / 2, 80, font, value_color, value, 0);
        /* The track follows the displayed rounded value, keeping identical updates pixel-stable. */
        am_scene_progress(&s_scene, x + 11, 159, 66, 4, value_color, TFT_RGB565(73, 62, 86), tenths * 10);
        reset_text(model, usage, utc, date, sizeof(date), clock, sizeof(clock));
        if (empty) {
            snprintf(date, sizeof(date), "--");
            snprintf(clock, sizeof(clock), "--");
        }
        if (i) {
            const struct am_font *date_font = am_text_width(&g_am_font_usage_reset, date) <= 80 ? &g_am_font_usage_reset : &g_am_font_reset;
            am_scene_text(&s_scene, x + 4, 168, 80, date_font, TFT_RGB565(199, 186, 216), date, 0);
        }
        am_scene_text(&s_scene, x + 4, i ? 184 : 175, 80, &g_am_font_body, TFT_RGB565(199, 186, 216), clock, 0);
    }
}

static void boot_page(void)
{
    am_scene_bitmap(&s_scene, 85, 45, 70, 70, g_am_brand_pixels);
    centered(118, 180, &g_am_font_connect, C_FG, "AgentMate");
    centered(155, 180, &g_am_font_label, TFT_RGB565(145, 160, 207), AM_PRODUCT_MODEL);
    centered(184, 140, &g_am_font_body, TFT_RGB565(194, 202, 232), AM_SOFTWARE_VERSION);
}

static esp_err_t render_factory_page(am_app_t *app, am_test_t test, uint8_t led_index, rgb_led_color_t led,
                                      uint8_t brightness, uint8_t backlight, uint8_t font_index)
{
    memset(&s_scene, 0, sizeof(s_scene));
    s_scene.body_alpha = 255;
    char title[32], detail[32];
    if (test == AM_TEST_FONT) {
        centered(51, 180, &g_am_font_factory_title, C_FG, "字体样张");
        centered(132, 190, &g_am_font_factory_body, C_FG, "工作中 等待输入");
        snprintf(detail, sizeof(detail), "AaBbCc / %u px", factory_font(font_index)->height);
        centered(166, 180, &g_am_font_label, TFT_RGB565(196, 203, 234), detail);
    } else if (test == AM_TEST_LED) {
        if (led_index == 16) {
            snprintf(title, sizeof(title), "LED 全灯");
        } else {
            snprintf(title, sizeof(title), "LED %02u / 16", led_index + 1);
        }
        centered(51, 200, &g_am_font_factory_title, C_FG, title);
        snprintf(detail, sizeof(detail), "RGB #%02X%02X%02X", led.r, led.g, led.b);
        centered(98, 190, &g_am_font_label, TFT_RGB565(196, 203, 234), detail);
        snprintf(detail, sizeof(detail), "亮度 %u%%", am_brightness_to_percent(brightness));
        centered(124, 180, &g_am_font_body, TFT_RGB565(196, 203, 234), detail);
        centered(177, 180, &g_am_font_factory_body, TFT_RGB565(143, 156, 194), "灯珠测试");
    } else if (test == AM_TEST_BACKLIGHT) {
        snprintf(title, sizeof(title), "背光 %u", backlight);
        centered(51, 180, &g_am_font_factory_title, C_FG, title);
        centered(98, 180, &g_am_font_label, TFT_RGB565(196, 203, 234), "32 / 128 / 255");
        centered(177, 180, &g_am_font_factory_body, TFT_RGB565(143, 156, 194), "亮度测试");
    } else {
        centered(51, 180, &g_am_font_factory_title, C_FG, "产测模式");
        centered(98, 190, &g_am_font_label, TFT_RGB565(196, 203, 234), "AT+TESTAUTO=1");
        centered(126, 190, &g_am_font_label, TFT_RGB565(196, 203, 234), "AT+MODE=NORMAL");
        centered(177, 180, &g_am_font_factory_body, TFT_RGB565(143, 156, 194), "串口就绪");
    }
    return am_scene_render(app, NULL, &s_scene, true);
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

static void ota_page(const struct am_ota_view *view)
{
    uint16_t color = view->state == AM_OTA_FAILED ? TFT_RGB565(250, 80, 95) :
                     view->state == AM_OTA_READY ? TFT_RGB565(80, 230, 160) : TFT_RGB565(85, 170, 250);
    centered(48, 184, &g_am_font_label, TFT_RGB565(175, 192, 213), "FIRMWARE UPDATE");
    centered(78, 190, &g_am_font_label, color, view->version);
    char progress[16];
    unsigned percent = view->size ? (unsigned)((uint64_t)view->received * 100 / view->size) : 0;
    snprintf(progress, sizeof(progress), "%u%%", percent);
    centered(108, 190, &g_am_font_quota, color, progress);
    am_scene_progress(&s_scene, 44, 153, 152, 6, color, TFT_RGB565(22, 36, 53), percent * 100);
    const char *status = view->state == AM_OTA_READY ? "WAITING FOR COMMIT" :
                         view->state == AM_OTA_VERIFYING ? "VERIFYING" : view->state == AM_OTA_FAILED ? "UPDATE FAILED" : "RECEIVING";
    centered(173, 172, &g_am_font_small, color, status);
    if (view->state == AM_OTA_FAILED) {
        centered(193, 150, &g_am_font_small, TFT_RGB565(175, 192, 213), view->error + 4);
    }
}

static esp_err_t ota_led(am_app_t *app, const struct am_ota_view *view, int64_t now)
{
    unsigned lit = view->size ? (unsigned)(((uint64_t)view->received * 16 + view->size - 1) / view->size) : 0;
    rgb_led_color_t color = {.r = 30, .g = 110, .b = 230};
    if (view->state == AM_OTA_READY) {
        color = (rgb_led_color_t){.r = 30, .g = 210, .b = 100};
        lit = 16;
    } else if (view->state == AM_OTA_FAILED) {
        color = (rgb_led_color_t){.r = 230, .g = 30, .b = 40};
        lit = 16;
    } else if (view->state == AM_OTA_VERIFYING) {
        color.b = (uint8_t)(100 + (now / 100000 % 8) * 18);
        lit = 16;
    }
    for (unsigned i = 0; i < 16; ++i) {
        esp_err_t err = rgb_led_strip_set_pixel(app->strip, i, i < lit ? color : (rgb_led_color_t){.r = 0, .g = 2, .b = 6});
        if (err != ESP_OK) {
            return err;
        }
    }
    return rgb_led_strip_show(app->strip);
}

static void ui_health(am_app_t *app, bool healthy)
{
    if (healthy) {
        xSemaphoreTake(app->model_lock, portMAX_DELAY);
        app->ui_heartbeat_us = esp_timer_get_time();
        xSemaphoreGive(app->model_lock);
    }
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
    bool ota_before = false;
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
        bool ota = s_snapshot.ota.state == AM_OTA_RECEIVING || s_snapshot.ota.state == AM_OTA_VERIFYING ||
                   s_snapshot.ota.state == AM_OTA_READY ||
                   (s_snapshot.ota.state == AM_OTA_FAILED && now - s_snapshot.ota.changed_us < 5000000);
        full = full || ota != ota_before;
        ota_before = ota;
        if (ota) {
            ota_page(&s_snapshot.ota);
            xSemaphoreTake(app->display_lock, portMAX_DELAY);
            esp_err_t err = am_scene_render(app, &s_previous, &s_scene, full);
            esp_err_t led_err = ota_led(app, &s_snapshot.ota, now);
            xSemaphoreGive(app->display_lock);
            ui_health(app, err == ESP_OK && led_err == ESP_OK);
            if (err == ESP_OK) {
                rotation_before = rotation;
            }
            page_before = AM_PAGE_BOOT;
            info_before = false;
            transition_started = 0;
            s_led_state.initialized = false;
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }
        if (!boot_done) {
            boot_page();
            s_scene.body_alpha = age < 300000 ? (uint8_t)((age / 80000) * 80 * 255 / 300) : 255;
            xSemaphoreTake(app->display_lock, portMAX_DELAY);
            esp_err_t err = am_scene_render(app, &s_previous, &s_scene, full);
            if (err == ESP_OK) {
                rotation_before = rotation;
            }
            esp_err_t led_err = rgb_led_strip_clear(app->strip);
            if (led_err == ESP_OK) {
                led_err = rgb_led_strip_show(app->strip);
            }
            xSemaphoreGive(app->display_lock);
            ui_health(app, err == ESP_OK && led_err == ESP_OK);
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
            ui_health(app, factory_key == key && rotation_before == rotation);
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }
        if (factory_key != UINT64_MAX) {
            full = true;
            factory_key = UINT64_MAX;
        }
        struct am_ui_selection selection;
        am_ui_select(&s_snapshot, &s_schedule, now, &selection);
        if (connect_first) {
            if (!selection.clock_preview) {
                selection.page = AM_PAGE_CONNECT;
            }
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
        if (selection.page != AM_PAGE_TASK && selection.page != AM_PAGE_RESULT && selection.page != AM_PAGE_BATTERY) {
            business_header(s_snapshot.link == AM_LINK_ONLINE && selection.page != AM_PAGE_CONNECT);
        }
        switch (selection.page) {
        case AM_PAGE_CONNECT:
            connection_page(&s_snapshot);
            break;
        case AM_PAGE_TASK:
        case AM_PAGE_RESULT:
            session_page(&s_snapshot, &s_snapshot.sessions[selection.slots[selection.current]], &selection, now, s_snapshot.remote_generation, utc);
            break;
        case AM_PAGE_USAGE:
            usage_page(&s_snapshot, &selection, utc);
            break;
        case AM_PAGE_BATTERY:
            battery_page(&s_snapshot, utc);
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
        ui_health(app, err == ESP_OK && led_err == ESP_OK);
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
