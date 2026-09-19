/**
 * @file app_main.c
 * @brief AGENTMATE RGB LED strip smoke test.
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_log.h"

#include "rgb_led_strip.h"

#define AGENTMATE_RGB_DATA_GPIO GPIO_NUM_1
#define AGENTMATE_RGB_LED_COUNT 16U
#define AGENTMATE_RGB_BRIGHTNESS RGB_LED_STRIP_DEFAULT_BRIGHTNESS
#define AGENTMATE_RGB_STEP_DELAY_MS 80U
#define AGENTMATE_RGB_COLOR_DELAY_MS 700U

static const char *TAG = "agentmate_rgb_test";

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

void app_main(void)
{
    rgb_led_strip_handle_t strip = NULL;
    ESP_ERROR_CHECK(rgb_led_strip_create(AGENTMATE_RGB_DATA_GPIO,
                                         AGENTMATE_RGB_LED_COUNT, &strip));
    ESP_ERROR_CHECK(rgb_led_strip_set_brightness(strip, AGENTMATE_RGB_BRIGHTNESS));
    ESP_ERROR_CHECK(rgb_led_strip_clear(strip));
    ESP_ERROR_CHECK(rgb_led_strip_show(strip));

    ESP_LOGI(TAG, "AGENTMATE RGB test started: GPIO=%d, LEDs=%u, brightness=%u/255",
             AGENTMATE_RGB_DATA_GPIO, AGENTMATE_RGB_LED_COUNT,
             AGENTMATE_RGB_BRIGHTNESS);

    for (;;) {
        ESP_ERROR_CHECK(show_color(strip, RGB_LED_COLOR_RED, "red"));
        vTaskDelay(pdMS_TO_TICKS(AGENTMATE_RGB_COLOR_DELAY_MS));

        ESP_ERROR_CHECK(show_color(strip, RGB_LED_COLOR_GREEN, "green"));
        vTaskDelay(pdMS_TO_TICKS(AGENTMATE_RGB_COLOR_DELAY_MS));

        ESP_ERROR_CHECK(show_color(strip, RGB_LED_COLOR_BLUE, "blue"));
        vTaskDelay(pdMS_TO_TICKS(AGENTMATE_RGB_COLOR_DELAY_MS));

        ESP_ERROR_CHECK(show_color(strip, RGB_LED_COLOR_WHITE, "white"));
        vTaskDelay(pdMS_TO_TICKS(AGENTMATE_RGB_COLOR_DELAY_MS));

        ESP_ERROR_CHECK(show_chase(strip));

        ESP_LOGI(TAG, "clearing strip");
        ESP_ERROR_CHECK(rgb_led_strip_clear(strip));
        ESP_ERROR_CHECK(rgb_led_strip_show(strip));
        vTaskDelay(pdMS_TO_TICKS(AGENTMATE_RGB_COLOR_DELAY_MS));
    }
}
