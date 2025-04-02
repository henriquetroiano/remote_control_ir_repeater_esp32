#ifndef LED_H
#define LED_H

#include <stdbool.h>

/**
 * @brief Inicializa o LED (configura GPIO).
 */
void led_init(void);

/**
 * @brief Alterna (toggle) o estado do LED (se estava OFF, liga; se estava ON, desliga).
 */
void led_toggle(void);

/**
 * @brief Retorna o estado atual do LED (true = ON, false = OFF).
 */
bool led_get_state(void);

#endif
