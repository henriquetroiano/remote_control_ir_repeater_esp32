#ifndef INFRARED_H
#define INFRARED_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Inicializa GPIOs e fila de IR
void infrared_init(void);

// Task que escuta sinal IR e envia para a fila
void infrared_task(void *arg);

// Task que escuta a fila e transmite IR
void ir_send_task(void *arg);

void ir_send_raw(const uint32_t *pulses, int len);

#ifdef __cplusplus
}
#endif

#endif // INFRARED_H
