idf_component_register(
    SRCS "ui_render.c" "ui_assets.c" "ui_state.c" "text.c" "app_main.c" "ui.c" "protocol.c"
        "ota.c" "xmodem.c" "usage.c" "led.c" "status.c"
    INCLUDE_DIRS "."
    REQUIRES esp_timer rgb_led_strip tft_display lcd_font nvs_flash
             esp_driver_uart esp_rom bootloader_support esp_hw_support app_update mbedtls
)

idf_build_get_property(project_ver PROJECT_VER)
if(NOT project_ver MATCHES "^[0-9]+\\.[0-9]+$")
    message(FATAL_ERROR "Project version must be major.minor, got \"${project_ver}\"")
endif()
target_compile_definitions(${COMPONENT_LIB} PRIVATE "AM_SOFTWARE_VERSION=\"v${project_ver}\"")

idf_component_get_property(lcd_font_lib lcd_font COMPONENT_LIB)
target_compile_definitions(${lcd_font_lib} PRIVATE
    CONFIG_LCD_FONT_ASCII_10X18=1
    CONFIG_LCD_FONT_SUN_ASCII_12X22=1
    CONFIG_LCD_FONT_TER_ASCII_16X32=1
)
