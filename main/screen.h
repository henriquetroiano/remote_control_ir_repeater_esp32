#ifndef SCREEN_H
#define SCREEN_H

#include <esp_err.h>

/**
 * @brief Inicializa o display. Se não for encontrado, continua sem travar,
 *        mas as funções de desenho ficam inoperantes.
 */
esp_err_t screen_init(void);

/**
 * @brief Limpa a tela (fundo preto).
 */
void screen_clear(void);

/**
 * @brief Envia o buffer local (s_oled_buffer) para o display.
 */
void screen_update_default(void);

/**
 * @brief Desenha uma string (fonte 6x8), pixel ON = bit=1, ou seja, 
 *        texto “branco” em fundo “preto”.
 * 
 * @param x Posição X do caractere (coluna).
 * @param y Posição Y do caractere (linha).
 * @param text Texto a ser desenhado (sem quebra de linha).
 */
void screen_draw_string(int x, int y, const char *text);

/**
 * @brief Limpa a tela com fundo branco (todos os bits em 1).
 */
void screen_clear_default(void);

/**
 * @brief Desenha uma string (fonte 6x8), porém escalonada e “preta” no buffer
 *        (ou seja, limpa o bit em vez de setar).
 *        Ideal para quando o fundo já estiver branco.
 * 
 * @param x Posição X do caractere superior-esquerdo.
 * @param y Posição Y do caractere superior-esquerdo.
 * @param text Texto a ser desenhado.
 * @param scale Fator de escala (1 = normal, 2 = dobro, etc).
 */
void screen_draw_string_scaled(int x, int y, const char *text, int scale);

int screen_get_string_width(const char *text, int scale); 

#endif // SCREEN_H
