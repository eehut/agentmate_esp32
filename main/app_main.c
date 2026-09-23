/**
 * @file app_main.c
 * @brief AGENTMATE RGB LED strip and GC9A01 TFT smoke test.
 */

#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_timer.h"

#include "rgb_led_strip.h"
#include "tft_display.h"

#define AGENTMATE_RGB_DATA_GPIO GPIO_NUM_1
#define AGENTMATE_RGB_LED_COUNT 16U
#define AGENTMATE_RGB_BRIGHTNESS RGB_LED_STRIP_DEFAULT_BRIGHTNESS
#define AGENTMATE_RGB_STEP_DELAY_MS 80U
#define AGENTMATE_RGB_COLOR_DELAY_MS 700U

#define AGENTMATE_TFT_SCLK_GPIO GPIO_NUM_6
#define AGENTMATE_TFT_MOSI_GPIO GPIO_NUM_7
#define AGENTMATE_TFT_CS_GPIO GPIO_NUM_10
#define AGENTMATE_TFT_DC_GPIO GPIO_NUM_3
#define AGENTMATE_TFT_RST_GPIO GPIO_NUM_4
#define AGENTMATE_TFT_BACKLIGHT_GPIO GPIO_NUM_5
#define AGENTMATE_TFT_FRAME_DELAY_MS 450U
#define AGENTMATE_TFT_ANIMATION_DELAY_MS 25U
#define AGENTMATE_TFT_TIMING_SCALE 2

static const char *TAG = "agentmate_rgb_test";

static const uint8_t *timing_glyph(char character)
{
    static const uint8_t blank[] = {0, 0, 0, 0, 0, 0, 0};
    static const uint8_t digits[][7] = {
        {0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E}, /* 0 */
        {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E}, /* 1 */
        {0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F}, /* 2 */
        {0x1E, 0x01, 0x01, 0x0E, 0x01, 0x01, 0x1E}, /* 3 */
        {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02}, /* 4 */
        {0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E}, /* 5 */
        {0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E}, /* 6 */
        {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08}, /* 7 */
        {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E}, /* 8 */
        {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C}, /* 9 */
    };
    static const uint8_t f[] = {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10};
    static const uint8_t p[] = {0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10};
    static const uint8_t s[] = {0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E};
    static const uint8_t m[] = {0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11};

    if (character >= '0' && character <= '9') {
        return digits[character - '0'];
    }
    switch (character) {
    case 'F': return f;
    case 'P': return p;
    case 'S': return s;
    case 'M': return m;
    default: return blank;
    }
}

static esp_err_t draw_timing_text(tft_display_handle_t display, int x, int y, const char *text,
                                  uint16_t color)
{
    const int scale = AGENTMATE_TFT_TIMING_SCALE;
    for (; *text != '\0'; ++text, x += 6 * scale) {
        const uint8_t *glyph = timing_glyph(*text);
        for (int row = 0; row < 7; ++row) {
            for (int column = 0; column < 5; ++column) {
                if (glyph[row] & (1U << (4 - column))) {
                    ESP_RETURN_ON_ERROR(tft_display_fill_rect(display, x + column * scale,
                                                               y + row * scale, scale, scale, color), TAG,
                                        "draw timing text failed");
                }
            }
        }
    }
    return ESP_OK;
}

static esp_err_t show_full_frame_timing(tft_display_handle_t display, uint16_t background,
                                        int64_t elapsed_us)
{
    const int overlay_width = 76;
    const int overlay_height = 34;
    const int overlay_x = (int)(tft_display_width(display) - overlay_width) / 2;
    const int overlay_y = (int)(tft_display_height(display) - overlay_height) / 2;
    char fps_text[8];
    char ms_text[8];
    const unsigned long fps = (unsigned long)((1000000LL + elapsed_us / 2) / elapsed_us);
    const unsigned long milliseconds = (unsigned long)((elapsed_us + 500) / 1000);
    const uint16_t foreground = background == TFT_COLOR_BLACK ? TFT_COLOR_WHITE : TFT_COLOR_BLACK;

    snprintf(fps_text, sizeof(fps_text), "FPS %lu", fps);
    snprintf(ms_text, sizeof(ms_text), "MS %lu", milliseconds);
    ESP_RETURN_ON_ERROR(tft_display_fill_rect(display, overlay_x, overlay_y, overlay_width,
                                               overlay_height, background), TAG,
                        "clear timing overlay failed");
    ESP_RETURN_ON_ERROR(draw_timing_text(display, overlay_x + 4, overlay_y + 3, fps_text, foreground), TAG,
                        "draw FPS overlay failed");
    return draw_timing_text(display, overlay_x + 4, overlay_y + 19, ms_text, foreground);
}

static esp_err_t show_color(rgb_led_strip_handle_t strip, rgb_led_color_t color,
                            const char *name)
{
    ESP_LOGI(TAG, "showing %s", name);
    ESP_RETURN_ON_ERROR(rgb_led_strip_fill(strip, color), TAG, "failed to fill %s", name);
    return rgb_led_strip_show(strip);
}

static esp_err_t show_chase(rgb_led_strip_handle_t strip)
{
    ESP_LOGI(TAG, "showing green chase");

    for (size_t index = 0; index < AGENTMATE_RGB_LED_COUNT; ++index) {
        ESP_RETURN_ON_ERROR(rgb_led_strip_clear(strip), TAG, "failed to clear chase frame");
        ESP_RETURN_ON_ERROR(rgb_led_strip_set_pixel(strip, index, RGB_LED_COLOR_GREEN),
                            TAG, "failed to set chase pixel %u", (unsigned)index);
        ESP_RETURN_ON_ERROR(rgb_led_strip_show(strip), TAG, "failed to show chase frame");
        vTaskDelay(pdMS_TO_TICKS(AGENTMATE_RGB_STEP_DELAY_MS));
    }

    return ESP_OK;
}

static esp_err_t run_rgb_smoke_test(rgb_led_strip_handle_t strip)
{
    ESP_RETURN_ON_ERROR(show_color(strip, RGB_LED_COLOR_RED, "red"), TAG, "red failed");
    vTaskDelay(pdMS_TO_TICKS(AGENTMATE_RGB_COLOR_DELAY_MS));
    ESP_RETURN_ON_ERROR(show_color(strip, RGB_LED_COLOR_GREEN, "green"), TAG, "green failed");
    vTaskDelay(pdMS_TO_TICKS(AGENTMATE_RGB_COLOR_DELAY_MS));
    ESP_RETURN_ON_ERROR(show_color(strip, RGB_LED_COLOR_BLUE, "blue"), TAG, "blue failed");
    vTaskDelay(pdMS_TO_TICKS(AGENTMATE_RGB_COLOR_DELAY_MS));
    ESP_RETURN_ON_ERROR(show_color(strip, RGB_LED_COLOR_WHITE, "white"), TAG, "white failed");
    vTaskDelay(pdMS_TO_TICKS(AGENTMATE_RGB_COLOR_DELAY_MS));
    ESP_RETURN_ON_ERROR(show_chase(strip), TAG, "chase failed");
    ESP_RETURN_ON_ERROR(rgb_led_strip_clear(strip), TAG, "clear failed");
    return rgb_led_strip_show(strip);
}

static esp_err_t run_tft_color_cycle(tft_display_handle_t display)
{
    static const struct {
        uint16_t color;
        const char *name;
    } colors[] = {
        {TFT_COLOR_RED, "red"}, {TFT_COLOR_GREEN, "green"}, {TFT_COLOR_BLUE, "blue"},
        {TFT_COLOR_WHITE, "white"}, {TFT_COLOR_BLACK, "black"},
    };

    for (size_t i = 0; i < sizeof(colors) / sizeof(colors[0]); ++i) {
        ESP_LOGI(TAG, "TFT full-screen %s", colors[i].name);
        const int64_t start_us = esp_timer_get_time();
        ESP_RETURN_ON_ERROR(tft_display_fill(display, colors[i].color), TAG, "TFT fill failed");
        const int64_t elapsed_us = esp_timer_get_time() - start_us;
        ESP_LOGI(TAG, "TFT full-frame: %lld us (%lu FPS)", elapsed_us,
                 (unsigned long)((1000000LL + elapsed_us / 2) / elapsed_us));
        ESP_RETURN_ON_ERROR(show_full_frame_timing(display, colors[i].color, elapsed_us), TAG,
                            "show frame timing failed");
        vTaskDelay(pdMS_TO_TICKS(AGENTMATE_TFT_FRAME_DELAY_MS));
    }
    return ESP_OK;
}

static esp_err_t run_tft_pattern_test(tft_display_handle_t display)
{
    const int width = tft_display_width(display);
    const int height = tft_display_height(display);
    const uint16_t bars[] = {
        TFT_COLOR_RED, TFT_RGB565(255, 128, 0), TFT_RGB565(255, 255, 0),
        TFT_COLOR_GREEN, TFT_RGB565(0, 255, 255), TFT_COLOR_BLUE,
    };
    const int bar_width = width / (int)(sizeof(bars) / sizeof(bars[0]));

    ESP_LOGI(TAG, "TFT RGB color bars and orientation markers");
    for (size_t i = 0; i < sizeof(bars) / sizeof(bars[0]); ++i) {
        const int x = (int)i * bar_width;
        const int current_width = i + 1 == sizeof(bars) / sizeof(bars[0]) ? width - x : bar_width;
        ESP_RETURN_ON_ERROR(tft_display_fill_rect(display, x, 0, current_width, height, bars[i]), TAG,
                            "TFT bar failed");
    }
    ESP_RETURN_ON_ERROR(tft_display_fill_rect(display, 0, 0, 24, 24, TFT_COLOR_WHITE), TAG,
                        "TFT top-left marker failed");
    ESP_RETURN_ON_ERROR(tft_display_fill_rect(display, width - 24, height - 24, 24, 24, TFT_COLOR_BLACK), TAG,
                        "TFT bottom-right marker failed");
    vTaskDelay(pdMS_TO_TICKS(AGENTMATE_TFT_FRAME_DELAY_MS * 2U));
    return ESP_OK;
}

static esp_err_t run_tft_local_refresh_test(tft_display_handle_t display)
{
    const int width = tft_display_width(display);
    const int height = tft_display_height(display);
    const int square_size = 24;
    const int steps = 48;
    int previous_x = 0;
    int previous_y = height / 2 - square_size / 2;

    ESP_LOGI(TAG, "TFT local refresh animation");
    ESP_RETURN_ON_ERROR(tft_display_fill(display, TFT_RGB565(8, 8, 24)), TAG, "TFT background failed");
    for (int step = 0; step < steps; ++step) {
        const int x = step * (width - square_size) / (steps - 1);
        const int y = height / 2 - square_size / 2 + ((step % 12) - 6) * 4;
        if (step > 0) {
            ESP_RETURN_ON_ERROR(tft_display_fill_rect(display, previous_x, previous_y, square_size, square_size,
                                                       TFT_RGB565(8, 8, 24)), TAG, "TFT erase failed");
        }
        ESP_RETURN_ON_ERROR(tft_display_fill_rect(display, x, y, square_size, square_size,
                                                   TFT_RGB565(255, 32, 160)), TAG, "TFT square failed");
        previous_x = x;
        previous_y = y;
        vTaskDelay(pdMS_TO_TICKS(AGENTMATE_TFT_ANIMATION_DELAY_MS));
    }
    return ESP_OK;
}

void app_main(void)
{
    rgb_led_strip_handle_t strip = NULL;
    tft_display_handle_t display = NULL;
    ESP_ERROR_CHECK(rgb_led_strip_create(AGENTMATE_RGB_DATA_GPIO,
                                         AGENTMATE_RGB_LED_COUNT, &strip));
    ESP_ERROR_CHECK(rgb_led_strip_set_brightness(strip, AGENTMATE_RGB_BRIGHTNESS));
    ESP_ERROR_CHECK(rgb_led_strip_clear(strip));
    ESP_ERROR_CHECK(rgb_led_strip_show(strip));
    const tft_display_config_t tft_config = {
        .controller = TFT_CONTROLLER_GC9A01,
        .spi_host = SPI2_HOST,
        .sclk_gpio = AGENTMATE_TFT_SCLK_GPIO,
        .mosi_gpio = AGENTMATE_TFT_MOSI_GPIO,
        .cs_gpio = AGENTMATE_TFT_CS_GPIO,
        .dc_gpio = AGENTMATE_TFT_DC_GPIO,
        .reset_gpio = AGENTMATE_TFT_RST_GPIO,
        .spi_clock_hz = 40 * 1000 * 1000,
        .max_transfer_bytes = 4096,
        .backlight_gpio = AGENTMATE_TFT_BACKLIGHT_GPIO,
        .backlight_speed_mode = LEDC_LOW_SPEED_MODE,
        .backlight_timer = LEDC_TIMER_0,
        .backlight_channel = LEDC_CHANNEL_0,
        .backlight_pwm_hz = 5000,
        .initial_brightness = 192,
    };
    ESP_ERROR_CHECK(tft_display_create(&tft_config, &display));

    ESP_LOGI(TAG, "AGENTMATE RGB + TFT test started: RGB GPIO=%d, LEDs=%u, brightness=%u/255",
             AGENTMATE_RGB_DATA_GPIO, AGENTMATE_RGB_LED_COUNT,
             AGENTMATE_RGB_BRIGHTNESS);

    for (;;) {
        ESP_ERROR_CHECK(run_rgb_smoke_test(strip));
        ESP_ERROR_CHECK(run_tft_color_cycle(display));
        ESP_ERROR_CHECK(run_tft_pattern_test(display));
        ESP_ERROR_CHECK(run_tft_local_refresh_test(display));
    }
}
