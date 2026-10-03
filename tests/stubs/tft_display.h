#ifndef TEST_TFT_H
#define TEST_TFT_H
#include <stdint.h>
#include <stddef.h>
typedef int esp_err_t;
typedef void *tft_display_handle_t;
typedef int tft_rotation_t;
#define ESP_OK 0
#define TFT_RGB565(r,g,b) ((((r)&248)<<8)|(((g)&252)<<3)|((b)>>3))
#define TFT_COLOR_BLACK 0
#define TFT_COLOR_WHITE 65535
#define TFT_COLOR_RED TFT_RGB565(255,0,0)
#define TFT_COLOR_GREEN TFT_RGB565(0,255,0)
#define TFT_COLOR_BLUE TFT_RGB565(0,0,255)
esp_err_t tft_display_draw_bitmap_rgb565(tft_display_handle_t h,int x,int y,int w,int height,const uint16_t *p);
esp_err_t tft_display_fill(tft_display_handle_t h,uint16_t color);
esp_err_t tft_display_set_brightness(tft_display_handle_t h,uint8_t value);
esp_err_t tft_display_set_rotation(tft_display_handle_t h,tft_rotation_t value);
#endif
