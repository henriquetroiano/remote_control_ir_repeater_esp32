#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void infrared_init(void);
void infrared_task(void *arg);
void ir_send_task(void *arg);

// Envia um array de pulsos (em microssegundos)
void ir_send_raw(const uint32_t *pulses, int len);

#ifdef __cplusplus
}
#endif
