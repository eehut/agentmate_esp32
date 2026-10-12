#ifndef TEST_OTA_PLATFORM_H
#define TEST_OTA_PLATFORM_H
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include "tft_display.h"
#define UART_NUM_0 0
#define pdTRUE 1
#define pdPASS 1
#define configASSERT(value) assert(value)
#define ESP_ERR_OTA_ROLLBACK_INVALID_STATE 42
#define ESP_CHIP_ID_ESP32C3 5

typedef struct { uint16_t chip_id; uint8_t reserved[22]; } esp_image_header_t;
typedef struct { uint32_t address, length; } esp_image_segment_header_t;
typedef struct { char version[32], project_name[32]; uint8_t reserved[192]; } esp_app_desc_t;
typedef struct { uint32_t size; } esp_partition_t;
typedef uint32_t esp_ota_handle_t;
typedef enum { ESP_OTA_IMG_VALID, ESP_OTA_IMG_PENDING_VERIFY } esp_ota_img_states_t;
typedef struct { uint8_t sum; } mbedtls_sha256_context;
const esp_partition_t *esp_ota_get_next_update_partition(const esp_partition_t *partition);
const esp_partition_t *esp_ota_get_running_partition(void);
const esp_app_desc_t *esp_app_get_description(void);
esp_err_t esp_ota_begin(const esp_partition_t *partition, size_t length, esp_ota_handle_t *handle);
esp_err_t esp_ota_abort(esp_ota_handle_t handle);
esp_err_t esp_ota_write(esp_ota_handle_t handle, const void *bytes, size_t length);
esp_err_t esp_ota_end(esp_ota_handle_t handle);
esp_err_t esp_partition_read(const esp_partition_t *partition, size_t offset, void *bytes, size_t length);
esp_err_t esp_ota_get_partition_description(const esp_partition_t *partition, esp_app_desc_t *description);
esp_err_t esp_ota_set_boot_partition(const esp_partition_t *partition);
esp_err_t esp_ota_get_state_partition(const esp_partition_t *partition, esp_ota_img_states_t *state);
esp_err_t esp_ota_mark_app_valid_cancel_rollback(void);
esp_err_t esp_ota_mark_app_invalid_rollback_and_reboot(void);
void esp_restart(void);
esp_err_t uart_flush_input(int port);
int uart_write_bytes(int port, const void *bytes, size_t length);
void mbedtls_sha256_init(mbedtls_sha256_context *context);
void mbedtls_sha256_free(mbedtls_sha256_context *context);
int mbedtls_sha256_starts(mbedtls_sha256_context *context, int sha224);
int mbedtls_sha256_update(mbedtls_sha256_context *context, const uint8_t *data, size_t length);
int mbedtls_sha256_finish(mbedtls_sha256_context *context, uint8_t *digest);
void vTaskDelay(unsigned ticks);
void vTaskDelete(void *task);
int xTaskCreate(void (*entry)(void *), const char *name, unsigned stack, void *argument, unsigned priority, void *handle);
#endif
