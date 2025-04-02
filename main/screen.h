#ifndef SCREEN_H
#define SCREEN_H

#include <esp_err.h>

/**
 * @brief Inicializa I2C e o display SSD1306 (128x64).
 * 
 * @return esp_err_t (ESP_OK em caso de sucesso)
 */
esp_err_t screen_init(void);

/**
 * @brief Limpa todo o display (preenche com pixels OFF).
 */
void screen_clear(void);

/**
 * @brief Desenha uma string (texto) no buffer, na posição (x,y).
 *        Depois disso, chame screen_update() para enviar ao display.
 *
 * @param x Coluna inicial (0..127)
 * @param y Linha inicial (0..63) - de preferência múltiplo de 8 se for texto básico
 * @param text Ponteiro para string terminada em '\0'
 */
void screen_draw_string(int x, int y, const char *text);

/**
 * @brief Envia o framebuffer local para o display.
 */
void screen_update(void);

#endif
