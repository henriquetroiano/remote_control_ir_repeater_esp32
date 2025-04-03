#ifndef INFRARED_H
#define INFRARED_H

#include <stdint.h>

void infrared_init(void);
void infrared_task(void *arg);
void ir_send_raw(const uint32_t *pulses, int len);

#endif // INFRARED_H
