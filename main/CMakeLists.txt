idf_component_register(
    SRCS "app_main.c" "agentmate_ui.c" "agentmate_protocol.c"
    INCLUDE_DIRS "."
    REQUIRES esp_timer rgb_led_strip tft_display lcd_font nvs_flash mbedtls
             esp_driver_usb_serial_jtag esp_rom bootloader_support esp_hw_support
)
