#ifndef TEST_SEMPHR_H
#define TEST_SEMPHR_H
#include "FreeRTOS.h"
typedef void *SemaphoreHandle_t;
static inline int xSemaphoreTake(SemaphoreHandle_t handle, unsigned delay) { (void)handle; (void)delay; return 1; }
static inline int xSemaphoreGive(SemaphoreHandle_t handle) { (void)handle; return 1; }

#endif
