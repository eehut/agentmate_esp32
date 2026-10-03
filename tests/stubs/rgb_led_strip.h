#ifndef TEST_LED_H
#define TEST_LED_H
#include "tft_display.h"
typedef struct { uint8_t r,g,b; } rgb_led_color_t;
typedef void *rgb_led_strip_handle_t;
#define RGB_LED_COLOR_RED ((rgb_led_color_t){255,0,0})
#define RGB_LED_COLOR_GREEN ((rgb_led_color_t){0,255,0})
#define RGB_LED_COLOR_BLUE ((rgb_led_color_t){0,0,255})
#define RGB_LED_COLOR_WHITE ((rgb_led_color_t){255,255,255})
esp_err_t rgb_led_strip_set_brightness(rgb_led_strip_handle_t h,uint8_t v);
esp_err_t rgb_led_strip_clear(rgb_led_strip_handle_t h);
esp_err_t rgb_led_strip_show(rgb_led_strip_handle_t h);
esp_err_t rgb_led_strip_fill(rgb_led_strip_handle_t h,rgb_led_color_t v);
esp_err_t rgb_led_strip_set_pixel(rgb_led_strip_handle_t h,size_t i,rgb_led_color_t v);
esp_err_t rgb_led_strip_set_pixels(rgb_led_strip_handle_t h,size_t i,const rgb_led_color_t *v,size_t n);
#endif
