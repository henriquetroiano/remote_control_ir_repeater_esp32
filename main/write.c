// write.c
#include <string.h>
#include "screen.h"

void write_default(const char *comando)
{
    screen_clear(); // <-- Aqui trocamos de screen_clear_default() para screen_clear()

    {
        const char *texto = "AR CONDICIONADO";
        int scale = 1;
        int len = strlen(texto);
        int text_width = 6 * scale * len;
        int x = (128 - text_width) / 2;
        int y = 5;
        screen_draw_string(x, y, texto); // <-- Usa texto branco padrão sobre fundo preto
    }

    {
        const char *texto = "AGUARDANDO COMANDO";
        int scale = 1;
        int len = strlen(texto);
        int text_width = 6 * scale * len;
        int x = (128 - text_width) / 2;
        int y = 18;
        screen_draw_string(x, y, texto); // <-- Também usa padrão (branco sobre preto)
    }

    {
        int scale = 2;
        int max_width = 128;
        int x = 0;
    
        int len = strlen(comando);
        int partial_width = 0;
        int split_index = len; // posição para dividir, se necessário
    
        // Primeiro, tentamos calcular quantos caracteres cabem em 128px
        for (int i = 0; i < len; i++)
        {
            int char_width = (comando[i] == ' ') ? (1 * scale) : ((5 * scale) + (scale / 2));
    
            if (partial_width + char_width > max_width)
            {
                split_index = i;
                break;
            }
    
            partial_width += char_width;
        }
    
        if (split_index == len)
        {
            x = (max_width - partial_width) / 2;
            screen_draw_string_scaled(x, 30, comando, scale);
        }
        else
        {
            // Parte 1: até split_index
            char linha1[64] = {0};
            strncpy(linha1, comando, split_index);
            linha1[split_index] = '\0';
    
            // 🔽 remove traços ou espaços do final
            int end = strlen(linha1) - 1;
            while (end >= 0 && (linha1[end] == ' ' || linha1[end] == '-')) {
                linha1[end] = '\0';
                end--;
            }
    
            screen_draw_string_scaled(0, 30, linha1, scale);
    
            // Parte 2: o resto (com trim de espaço e traço no início)
            char *linha2 = comando + split_index;
            while (*linha2 == ' ' || *linha2 == '-') {
                linha2++;
            }
    
            int width2 = screen_get_string_width(linha2, scale);
            int x2 = (max_width - width2) / 2;
    
            screen_draw_string_scaled(x2, 47, linha2, scale);
        }
    }
    
    
    screen_update_default();
}
