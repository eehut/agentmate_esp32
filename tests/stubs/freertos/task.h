#ifndef TEST_TASK_H
#define TEST_TASK_H
static inline void vTaskDelay(unsigned ticks) { (void)ticks; }
static inline unsigned uxTaskGetStackHighWaterMark(void *task) { (void)task; return 2048; }

#endif
