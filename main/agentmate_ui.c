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
#
# Date       Version  Author       Description
# ---------- -------- ------------ -----------------------------------------------
# 2026-10-01 1.0      Liu Chuansen  Organized for project C style.
# 2026-10-01 1.1      Liu Chuansen  Added circle-aware partial screen updates.
# 2026-10-01 1.2      Codex         Added factory display and font diagnostics.
# 2026-10-02 1.3      Codex         Display transient task states and thinking.
#
#####################################################################################
*/

#include "agentmate_app.h"

#include <stdbool.h>
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
#define UI_SIZE 240
#define UI_CENTER_TWICE (UI_SIZE - 1)
#define UI_SAFE_RADIUS_TWICE 232
#define UI_FONT_WIDTH 8
#define UI_FONT_HEIGHT 16
#define UI_MAX_LINES 16
#define UI_ICON_X 18
#define UI_ICON_Y 66
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
    const uint16_t *icon;
    int icon_x;
    rgb_led_color_t led;
    bool led_valid;
} ui_frame_t;

static uint16_t s_text_pixels[UI_SIZE * 32];
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

static void header(const char *state, const char *clock)
{
    label(UI_CENTERED_X, 18, "AGENTMATE", ACCENT);
    label(UI_CENTERED_X, 37, state, MUTED);
    label(UI_CENTERED_X, 210, clock, MUTED);
}

static void clock_text(const am_model_t *model, char *out, size_t capacity)
{
    if (!model->clock_valid) {
        snprintf(out, capacity, "--:-- UTC");
        return;
    }
    time_t seconds = time(NULL);
    struct tm utc;
    if (gmtime_r(&seconds, &utc) == NULL) {
        snprintf(out, capacity, "--:-- UTC");
        return;
    }
    snprintf(out, capacity, "%02d:%02d:%02d UTC", utc.tm_hour, utc.tm_min, utc.tm_sec);
}

static bool advancing(const char *state)
{
    return strcmp(state, "working") == 0 || strcmp(state, "thinking") == 0 || strcmp(state, "waiting_for_input") == 0 ||
           strcmp(state, "permission_required") == 0;
}

static bool task_result_state(const char *state)
{
    return strcmp(state, "task_success") == 0 || strcmp(state, "task_canceled") == 0 || strcmp(state, "task_failed") == 0;
}

static const char *session_state_text(const char *state)
{
    if (strcmp(state, "thinking") == 0) {
        return "THINKING";
    }
    if (strcmp(state, "task_success") == 0) {
        return "TASK SUCCESS";
    }
    if (strcmp(state, "task_canceled") == 0) {
        return "TASK CANCELED";
    }
    if (strcmp(state, "task_failed") == 0) {
        return "TASK FAILED";
    }
    return state;
}

static void session_page(const am_session_t *session, const char *clock, int page, int total)
{
    char buffer[64];
    header("CONNECTED", clock);
    s_next_frame.icon = strcmp(session->provider, "claude") == 0 ? s_am_icon_claude : s_am_icon_codex;
    int left;
    int right;
    if (circle_span(UI_ICON_Y, AM_PROVIDER_ICON_SIZE, &left, &right) && left + AM_PROVIDER_ICON_SIZE - 1 <= right) {
        s_next_frame.icon_x = UI_ICON_X > left ? UI_ICON_X : left;
    } else {
        s_next_frame.icon = NULL;
    }
    snprintf(buffer, sizeof(buffer), "%s  %d/%d", session->provider, page, total);
    label(77, 68, buffer, FG);
    snprintf(buffer, sizeof(buffer), "SESSION %s", session->key);
    label(77, 89, buffer, MUTED);
    label(UI_CENTERED_X, 125, session_state_text(session->state),
          strcmp(session->state, "permission_required") == 0 || strcmp(session->state, "task_failed") == 0 ? DANGER
          : strcmp(session->state, "task_canceled") == 0 || strcmp(session->state, "waiting_for_input") == 0 ? WARNING
          : strcmp(session->state, "working") == 0 || strcmp(session->state, "thinking") == 0 ||
                  strcmp(session->state, "task_success") == 0 ? ACCENT
                                                               : FG);
    uint64_t duration = session->task_seconds;
    if (advancing(session->state) && session->task_base_us > 0) {
        duration += (esp_timer_get_time() - session->task_base_us) / 1000000;
    }
    snprintf(buffer, sizeof(buffer), "TASK %llum %02llus", (unsigned long long)(duration / 60), (unsigned long long)(duration % 60));
    label(UI_CENTERED_X, 147, buffer, FG);
    if (session->context_mode == 2) {
        snprintf(buffer, sizeof(buffer), "CTX %u.%02u%%", session->ctx_pct_bp / 100, session->ctx_pct_bp % 100);
        label(UI_CENTERED_X, 169, buffer, ACCENT);
        snprintf(buffer, sizeof(buffer), "TOK %lluk/%lluk", (unsigned long long)(session->ctx_used / 1000), (unsigned long long)(session->ctx_window / 1000));
        label(UI_CENTERED_X, 190, buffer, MUTED);
    } else if (session->context_mode == 1) {
        label(UI_CENTERED_X, 169, "CTX UNKNOWN", MUTED);
    } else {
        label(UI_CENTERED_X, 169, "CTX N/A", MUTED);
    }
    if (session->context_mode != 2 && !task_result_state(session->state) && strcmp(session->last_task, "-") != 0) {
        snprintf(buffer, sizeof(buffer), "LAST %s", session->last_task);
        label(UI_CENTERED_X, 190, buffer, MUTED);
    }
}

static void usage_page(const am_model_t *model, const char *clock)
{
    header("PROVIDER USAGE", clock);
    int shown = 0;
    for (int i = 0; i < AM_MAX_USAGE_ENTRIES && shown < 4; ++i) {
        const am_usage_t *usage = &model->usages[i];
        if (!usage->used) {
            continue;
        }
        char buffer[32];
        snprintf(buffer, sizeof(buffer), "%s %s", usage->provider, usage->window);
        label(UI_CENTERED_X, 70 + shown * 32, buffer, FG);
        if (usage->used_bp >= 0) {
            snprintf(buffer, sizeof(buffer), "%s %d.%02d%%", usage->metric, (int)(usage->used_bp / 100), (int)(usage->used_bp % 100));
        } else {
            snprintf(buffer, sizeof(buffer), "%s UNKNOWN", usage->metric);
        }
        label(UI_CENTERED_X, 86 + shown * 32, buffer, ACCENT);
        shown++;
    }
    if (!shown) {
        label(UI_CENTERED_X, 100, "NO USAGE DATA", MUTED);
    }
}

static void status_page(const am_model_t *model, const char *clock, const char *device_id)
{
    header(model->link == AM_LINK_DEGRADED ? "TIME UNSYNCED" : "CONNECTED", clock);
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "SESSIONS %d/%d", model->session_count, AM_MAX_SESSIONS);
    label(UI_CENTERED_X, 76, buffer, FG);
    snprintf(buffer, sizeof(buffer), "USAGE %d/%d", model->usage_count, AM_MAX_USAGE_ENTRIES);
    label(UI_CENTERED_X, 100, buffer, FG);
    snprintf(buffer, sizeof(buffer), "DEVICE %.8s", device_id + 24);
    label(UI_CENTERED_X, 124, buffer, MUTED);
    label(UI_CENTERED_X, 145, "FW 1.0", MUTED);
    if (model->clock_valid) {
        snprintf(buffer, sizeof(buffer), "SYNC AGE %llus", (unsigned long long)((esp_timer_get_time() - model->last_clock_sync_us) / 1000000));
    } else {
        snprintf(buffer, sizeof(buffer), "CLOCK WAITING");
    }
    label(UI_CENTERED_X, 166, buffer, ACCENT);
    snprintf(buffer, sizeof(buffer), "UPTIME %llum", (unsigned long long)(esp_timer_get_time() / 60000000));
    label(UI_CENTERED_X, 188, buffer, MUTED);
}

static void pairing_page(const am_model_t *model, const char *clock, const char *device_id)
{
    header("PAIRING", clock);
    label(UI_CENTERED_X, 71, model->pair_pin, WARNING);
    label(UI_CENTERED_X, 105, "ENTER CODE IN WEB", FG);
    char buffer[32];
    int remaining = (int)((model->pair_deadline_us - esp_timer_get_time() + 999999) / 1000000);
    if (remaining < 0) {
        remaining = 0;
    }
    snprintf(buffer, sizeof(buffer), "EXPIRES IN %ds", remaining);
    label(UI_CENTERED_X, 133, buffer, MUTED);
    snprintf(buffer, sizeof(buffer), "DEVICE %.8s", device_id + 24);
    label(UI_CENTERED_X, 162, buffer, MUTED);
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
        if (model->sessions[i].used && strcmp(model->sessions[i].state, "waiting_for_input") == 0) {
            return (rgb_led_color_t){.r = 255, .g = 185, .b = 0};
        }
    }
    for (int i = 0; i < AM_MAX_SESSIONS; ++i) {
        if (model->sessions[i].used && (strcmp(model->sessions[i].state, "working") == 0 || strcmp(model->sessions[i].state, "thinking") == 0)) {
            return RGB_LED_COLOR_GREEN;
        }
    }
    for (int i = 0; i < AM_MAX_SESSIONS; ++i) {
        if (model->sessions[i].used && strcmp(model->sessions[i].state, "task_failed") == 0) {
            return RGB_LED_COLOR_RED;
        }
    }
    for (int i = 0; i < AM_MAX_SESSIONS; ++i) {
        if (model->sessions[i].used && strcmp(model->sessions[i].state, "task_canceled") == 0) {
            return (rgb_led_color_t){.r = 255, .g = 185, .b = 0};
        }
    }
    for (int i = 0; i < AM_MAX_SESSIONS; ++i) {
        if (model->sessions[i].used && strcmp(model->sessions[i].state, "task_success") == 0) {
            return RGB_LED_COLOR_GREEN;
        }
    }
    return model->link == AM_LINK_DEGRADED ? RGB_LED_COLOR_RED : RGB_LED_COLOR_BLUE;
}

static const ui_line_t *find_line(const ui_frame_t *frame, int y)
{
    for (int i = 0; i < frame->line_count; ++i) {
        if (frame->lines[i].y == y) {
            return &frame->lines[i];
        }
    }
    return NULL;
}

static bool line_equal(const ui_line_t *first, const ui_line_t *second)
{
    return first->x == second->x && first->clear_x == second->clear_x && first->right == second->right && first->color == second->color &&
           strcmp(first->text, second->text) == 0;
}

static esp_err_t draw_line(am_app_t *app, const ui_line_t *line)
{
    int width = line->right - line->clear_x + 1;
    size_t pixels = (size_t)width * UI_FONT_HEIGHT;
    for (size_t i = 0; i < pixels; ++i) {
        s_text_pixels[i] = BG;
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
                    s_text_pixels[(size_t)row * width + text_x + (int)column * UI_FONT_WIDTH + bit] = line->color;
                }
            }
        }
    }
    return tft_display_draw_bitmap_rgb565(app->display, line->clear_x, line->y, width, UI_FONT_HEIGHT, s_text_pixels);
}

static bool same_icon(const ui_frame_t *first, const ui_frame_t *second)
{
    return first->icon == second->icon && first->icon_x == second->icon_x;
}

static esp_err_t render_frame(am_app_t *app, ui_frame_t *previous, const ui_frame_t *next, bool clear_screen)
{
    esp_err_t err;
    if (clear_screen) {
        err = tft_display_fill(app->display, BG);
        if (err != ESP_OK) {
            return err;
        }
        memset(previous, 0, sizeof(*previous));
    }
    for (int i = 0; i < previous->line_count; ++i) {
        const ui_line_t *old_line = &previous->lines[i];
        const ui_line_t *new_line = find_line(next, old_line->y);
        if (new_line == NULL || new_line->clear_x > old_line->clear_x || new_line->right < old_line->right) {
            err = tft_display_fill_rect(app->display, old_line->clear_x, old_line->y, old_line->right - old_line->clear_x + 1, UI_FONT_HEIGHT, BG);
            if (err != ESP_OK) {
                return err;
            }
        }
    }
    if (previous->icon != NULL && !same_icon(previous, next)) {
        err = tft_display_fill_rect(app->display, previous->icon_x, UI_ICON_Y, AM_PROVIDER_ICON_SIZE, AM_PROVIDER_ICON_SIZE, BG);
        if (err != ESP_OK) {
            return err;
        }
    }
    if (next->icon != NULL && !same_icon(previous, next)) {
        err = tft_display_draw_bitmap_rgb565(app->display, next->icon_x, UI_ICON_Y, AM_PROVIDER_ICON_SIZE, AM_PROVIDER_ICON_SIZE, next->icon);
        if (err != ESP_OK) {
            return err;
        }
    }
    for (int i = 0; i < next->line_count; ++i) {
        const ui_line_t *new_line = &next->lines[i];
        const ui_line_t *old_line = find_line(previous, new_line->y);
        if (old_line == NULL || !line_equal(old_line, new_line)) {
            err = draw_line(app, new_line);
            if (err != ESP_OK) {
                return err;
            }
        }
    }
    if (!previous->led_valid || !rgb_led_color_equal(previous->led, next->led)) {
        err = rgb_led_strip_fill(app->strip, next->led);
        if (err != ESP_OK) {
            return err;
        }
        err = rgb_led_strip_show(app->strip);
        if (err != ESP_OK) {
            return err;
        }
    }
    *previous = *next;
    return ESP_OK;
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
            s_text_pixels[i] = BG;
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
                        s_text_pixels[row * width + column * font->width + bit] = FG;
                    }
                }
            }
        }
        err = tft_display_draw_bitmap_rgb565(app->display, x, y, width, font->height, s_text_pixels);
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

void am_ui_run(am_app_t *app)
{
    static am_model_t s_snapshot;
    static ui_frame_t s_previous_frame;
    uint64_t previous_factory_key = UINT64_MAX;
    uint16_t previous_rotation = UINT16_MAX;
    char device_id[33];
    memcpy(device_id, app->device_id, sizeof(device_id));
    for (;;) {
        xSemaphoreTake(app->model_lock, portMAX_DELAY);
        memcpy(&s_snapshot, &app->model, sizeof(s_snapshot));
        uint16_t rotation = app->rotation;
        xSemaphoreGive(app->model_lock);
        int64_t now = esp_timer_get_time();
        if (s_snapshot.factory_mode) {
            uint32_t stage = s_snapshot.test == AM_TEST_AUTO ? (uint32_t)((now - s_snapshot.test_started_us) / 2000000) :
                             s_snapshot.test == AM_TEST_FONT ? (uint32_t)(now / 2000000) : 0;
            uint64_t key = ((uint64_t)s_snapshot.revision << 32) | stage;
            if (key != previous_factory_key) {
                xSemaphoreTake(app->display_lock, portMAX_DELAY);
                esp_err_t err = render_factory(app, &s_snapshot, now);
                xSemaphoreGive(app->display_lock);
                if (err == ESP_OK) {
                    previous_factory_key = key;
                }
            }
            vTaskDelay(pdMS_TO_TICKS(200));
            continue;
        }
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
        memset(&s_next_frame, 0, sizeof(s_next_frame));
        char clock[24];
        clock_text(&s_snapshot, clock, sizeof(clock));
        if (s_snapshot.link == AM_LINK_PAIRING && s_snapshot.pair_pin[0]) {
            pairing_page(&s_snapshot, clock, device_id);
        } else if (s_snapshot.link != AM_LINK_ONLINE && s_snapshot.link != AM_LINK_DEGRADED) {
            header(s_snapshot.link == AM_LINK_SYNCING     ? "SYNCING"
                   : s_snapshot.link == AM_LINK_UNPAIRED  ? "PAIR REQUIRED"
                   : s_snapshot.link == AM_LINK_HANDSHAKE ? "HANDSHAKE"
                                                          : "WAIT HOST",
                   clock);
            label(UI_CENTERED_X, 90, "USB SERIAL READY", FG);
            label(UI_CENTERED_X, 125, "AGENTMATE ESP32", MUTED);
        } else if (page < s_snapshot.session_count) {
            int ordinal = 0;
            for (int i = 0; i < AM_MAX_SESSIONS; ++i) {
                if (!s_snapshot.sessions[i].used) {
                    continue;
                }
                if (ordinal == page) {
                    session_page(&s_snapshot.sessions[i], clock, ordinal + 1, s_snapshot.session_count);
                    break;
                }
                ordinal++;
            }
        } else if (page == s_snapshot.session_count) {
            usage_page(&s_snapshot, clock);
        } else {
            status_page(&s_snapshot, clock, device_id);
        }
        s_next_frame.led = led_color(&s_snapshot);
        s_next_frame.led_valid = true;
        xSemaphoreTake(app->display_lock, portMAX_DELAY);
        esp_err_t err = render_frame(app, &s_previous_frame, &s_next_frame, rotation != previous_rotation);
        xSemaphoreGive(app->display_lock);
        if (err == ESP_OK) {
            previous_rotation = rotation;
        }
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}
