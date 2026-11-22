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
# Description: AgentMate ESP32-C3 application entry and hardware initialization
# Author: Liu Chuansen <liuchuansen@outlook.com>
#
#####################################################################################
#
# Revision History:
# 2026-10-12 2.0      Codex         Start pending-image boot health supervision.
#
# Date       Version  Author       Description
# ---------- -------- ------------ -----------------------------------------------
# 2026-10-01 1.0      Liu Chuansen  Organized for project C style.
#
#####################################################################################
*/

#include "app.h"
#include "ota.h"

#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/task.h"

#define RGB_GPIO GPIO_NUM_1
#define RGB_COUNT 16U
#define TFT_SCLK GPIO_NUM_6
#define TFT_MOSI GPIO_NUM_7
#define TFT_CS GPIO_NUM_10
#define TFT_DC GPIO_NUM_3
#define TFT_RST GPIO_NUM_4
#define TFT_BACKLIGHT GPIO_NUM_5

static am_app_t s_app;

static void protocol_task(void *argument)
{
    am_protocol_run((am_app_t *)argument);
    vTaskDelete(NULL);
}

static void ui_task(void *argument)
{
    am_ui_run((am_app_t *)argument);
    vTaskDelete(NULL);
}

void app_main(void)
{
    ESP_ERROR_CHECK(am_app_init(&s_app));
    am_ota_boot_watch(&s_app);
    ESP_ERROR_CHECK(rgb_led_strip_create(RGB_GPIO, RGB_COUNT, &s_app.strip));
    ESP_ERROR_CHECK(rgb_led_strip_set_brightness(s_app.strip, s_app.led_brightness));
    ESP_ERROR_CHECK(rgb_led_strip_clear(s_app.strip));
    ESP_ERROR_CHECK(rgb_led_strip_show(s_app.strip));
    const tft_display_config_t config = {
        .controller = TFT_CONTROLLER_GC9A01,
        .spi_host = SPI2_HOST,
        .sclk_gpio = TFT_SCLK,
        .mosi_gpio = TFT_MOSI,
        .cs_gpio = TFT_CS,
        .dc_gpio = TFT_DC,
        .reset_gpio = TFT_RST,
        .spi_clock_hz = 40 * 1000 * 1000,
        .max_transfer_bytes = 4096,
        .backlight_gpio = TFT_BACKLIGHT,
        .backlight_speed_mode = LEDC_LOW_SPEED_MODE,
        .backlight_timer = LEDC_TIMER_0,
        .backlight_channel = LEDC_CHANNEL_0,
        .backlight_pwm_hz = 5000,
        .initial_brightness = s_app.tft_brightness,
    };
    ESP_ERROR_CHECK(tft_display_create(&config, &s_app.display));
    ESP_ERROR_CHECK(am_ui_apply_rotation(&s_app, s_app.rotation));
    ESP_LOGI("agentmate", "firmware ready; device ID %.8s", s_app.device_id + 24);
    configASSERT(xTaskCreate(ui_task, "agentmate_ui", 6144, &s_app, 4, NULL) == pdPASS);
    configASSERT(xTaskCreate(protocol_task, "agentmate_at", 8192, &s_app, 5, NULL) == pdPASS);
}
