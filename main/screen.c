#include "screen.h"
#include <string.h> // memset
#include <stdio.h>  // printf, ...
#include <stdbool.h>
#include "driver/i2c.h"
#include "esp_log.h"

//
//  -------------------- Definições específicas do SSD1306 --------------------
//
#define SSD1306_ADDR 0x3C // Ajuste se for 0x3D
#define OLED_WIDTH 128
#define OLED_HEIGHT 64
#define OLED_PAGES (OLED_HEIGHT / 8)

// Pinos I2C (ajuste conforme sua placa)
#define I2C_SDA_PIN 21
#define I2C_SCL_PIN 22
#define I2C_PORT I2C_NUM_0
#define I2C_FREQ_HZ 100000

static const char *TAG = "SSD1306";

// Framebuffer local (128 * 8 = 1024 bytes)
static uint8_t s_oled_buffer[OLED_WIDTH * OLED_PAGES];

// Flag de presença do display
static bool s_screen_present = false;

// Fonte 6x8 (96 caracteres de 0x20 ' ' a 0x7F '~')
static const uint8_t ssd1306_font6x8[] = {
    // ASCII 0x20..0x7F (cada char 6 bytes => 6 colunas, 8 linhas)
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 0x20 ' '
    0x00, 0x00, 0x5F, 0x00, 0x00, 0x00, // 0x21 '!'
    0x00, 0x03, 0x00, 0x03, 0x00, 0x00, // 0x22 '"'
    0x14, 0x7F, 0x14, 0x7F, 0x14, 0x00, // 0x23 '#'
    0x24, 0x2A, 0x7F, 0x2A, 0x12, 0x00, // 0x24 '$'
    0x23, 0x13, 0x08, 0x64, 0x62, 0x00, // 0x25 '%'
    0x36, 0x49, 0x55, 0x22, 0x50, 0x00, // 0x26 '&'
    0x00, 0x05, 0x03, 0x00, 0x00, 0x00, // 0x27 '''
    0x00, 0x1C, 0x22, 0x41, 0x00, 0x00, // 0x28 '('
    0x00, 0x41, 0x22, 0x1C, 0x00, 0x00, // 0x29 ')'
    0x14, 0x08, 0x3E, 0x08, 0x14, 0x00, // 0x2A '*'
    0x08, 0x08, 0x3E, 0x08, 0x08, 0x00, // 0x2B '+'
    0x00, 0x50, 0x30, 0x00, 0x00, 0x00, // 0x2C ','
    0x08, 0x08, 0x08, 0x08, 0x08, 0x00, // 0x2D '-'
    0x00, 0x60, 0x60, 0x00, 0x00, 0x00, // 0x2E '.'
    0x20, 0x10, 0x08, 0x04, 0x02, 0x00, // 0x2F '/'
    0x3E, 0x51, 0x49, 0x45, 0x3E, 0x00, // 0x30 '0'
    0x00, 0x42, 0x7F, 0x40, 0x00, 0x00, // 0x31 '1'
    0x72, 0x49, 0x49, 0x49, 0x46, 0x00, // 0x32 '2'
    0x21, 0x49, 0x49, 0x49, 0x3E, 0x00, // 0x33 '3'
    0x18, 0x14, 0x12, 0x7F, 0x10, 0x00, // 0x34 '4'
    0x27, 0x45, 0x45, 0x45, 0x39, 0x00, // 0x35 '5'
    0x3C, 0x4A, 0x49, 0x49, 0x30, 0x00, // 0x36 '6'
    0x01, 0x71, 0x09, 0x05, 0x03, 0x00, // 0x37 '7'
    0x36, 0x49, 0x49, 0x49, 0x36, 0x00, // 0x38 '8'
    0x06, 0x49, 0x49, 0x29, 0x1E, 0x00, // 0x39 '9'
    0x00, 0x36, 0x36, 0x00, 0x00, 0x00, // 0x3A ':'
    0x00, 0x56, 0x36, 0x00, 0x00, 0x00, // 0x3B ';'
    0x08, 0x14, 0x22, 0x41, 0x00, 0x00, // 0x3C '<'
    0x14, 0x14, 0x14, 0x14, 0x14, 0x00, // 0x3D '='
    0x00, 0x41, 0x22, 0x14, 0x08, 0x00, // 0x3E '>'
    0x02, 0x01, 0x59, 0x09, 0x06, 0x00, // 0x3F '?'
    0x3E, 0x41, 0x5D, 0x59, 0x4E, 0x00, // 0x40 '@'
    0x7E, 0x11, 0x11, 0x11, 0x7E, 0x00, // 0x41 'A'
    0x7F, 0x49, 0x49, 0x49, 0x36, 0x00, // 0x42 'B'
    0x3E, 0x41, 0x41, 0x41, 0x22, 0x00, // 0x43 'C'
    0x7F, 0x41, 0x41, 0x22, 0x1C, 0x00, // 0x44 'D'
    0x7F, 0x49, 0x49, 0x49, 0x41, 0x00, // 0x45 'E'
    0x7F, 0x09, 0x09, 0x09, 0x01, 0x00, // 0x46 'F'
    0x3E, 0x41, 0x49, 0x49, 0x7A, 0x00, // 0x47 'G'
    0x7F, 0x08, 0x08, 0x08, 0x7F, 0x00, // 0x48 'H'
    0x00, 0x41, 0x7F, 0x41, 0x00, 0x00, // 0x49 'I'
    0x20, 0x40, 0x41, 0x3F, 0x01, 0x00, // 0x4A 'J'
    0x7F, 0x08, 0x14, 0x22, 0x41, 0x00, // 0x4B 'K'
    0x7F, 0x40, 0x40, 0x40, 0x40, 0x00, // 0x4C 'L'
    0x7F, 0x02, 0x04, 0x02, 0x7F, 0x00, // 0x4D 'M'
    0x7F, 0x04, 0x08, 0x10, 0x7F, 0x00, // 0x4E 'N'
    0x3E, 0x41, 0x41, 0x41, 0x3E, 0x00, // 0x4F 'O'
    0x7F, 0x09, 0x09, 0x09, 0x06, 0x00, // 0x50 'P'
    0x3E, 0x41, 0x51, 0x21, 0x5E, 0x00, // 0x51 'Q'
    0x7F, 0x09, 0x19, 0x29, 0x46, 0x00, // 0x52 'R'
    0x26, 0x49, 0x49, 0x49, 0x32, 0x00, // 0x53 'S'
    0x01, 0x01, 0x7F, 0x01, 0x01, 0x00, // 0x54 'T'
    0x3F, 0x40, 0x40, 0x40, 0x3F, 0x00, // 0x55 'U'
    0x1F, 0x20, 0x40, 0x20, 0x1F, 0x00, // 0x56 'V'
    0x7F, 0x20, 0x10, 0x20, 0x7F, 0x00, // 0x57 'W'
    0x63, 0x14, 0x08, 0x14, 0x63, 0x00, // 0x58 'X'
    0x07, 0x08, 0x70, 0x08, 0x07, 0x00, // 0x59 'Y'
    0x61, 0x51, 0x49, 0x45, 0x43, 0x00, // 0x5A 'Z'
    0x00, 0x7F, 0x41, 0x41, 0x00, 0x00, // 0x5B '['
    0x02, 0x04, 0x08, 0x10, 0x20, 0x00, // 0x5C '\'
    0x00, 0x41, 0x41, 0x7F, 0x00, 0x00, // 0x5D ']'
    0x04, 0x02, 0x01, 0x02, 0x04, 0x00, // 0x5E '^'
    0x80, 0x80, 0x80, 0x80, 0x80, 0x00, // 0x5F '_'
    0x00, 0x01, 0x02, 0x00, 0x00, 0x00, // 0x60 '`'
    0x20, 0x54, 0x54, 0x54, 0x38, 0x00, // 0x61 'a'
    0x7F, 0x28, 0x44, 0x44, 0x38, 0x00, // 0x62 'b'
    0x38, 0x44, 0x44, 0x44, 0x28, 0x00, // 0x63 'c'
    0x38, 0x44, 0x44, 0x28, 0x7F, 0x00, // 0x64 'd'
    0x38, 0x54, 0x54, 0x54, 0x18, 0x00, // 0x65 'e'
    0x08, 0x7E, 0x09, 0x01, 0x02, 0x00, // 0x66 'f'
    0x0C, 0x52, 0x52, 0x52, 0x3E, 0x00, // 0x67 'g'
    0x7F, 0x08, 0x04, 0x04, 0x78, 0x00, // 0x68 'h'
    0x00, 0x44, 0x7D, 0x40, 0x00, 0x00, // 0x69 'i'
    0x20, 0x40, 0x44, 0x3D, 0x00, 0x00, // 0x6A 'j'
    0x7F, 0x10, 0x28, 0x44, 0x00, 0x00, // 0x6B 'k'
    0x00, 0x41, 0x7F, 0x40, 0x00, 0x00, // 0x6C 'l'
    0x7C, 0x04, 0x38, 0x04, 0x78, 0x00, // 0x6D 'm'
    0x7C, 0x08, 0x04, 0x04, 0x78, 0x00, // 0x6E 'n'
    0x38, 0x44, 0x44, 0x44, 0x38, 0x00, // 0x6F 'o'
    0x7C, 0x14, 0x14, 0x14, 0x08, 0x00, // 0x70 'p'
    0x08, 0x14, 0x14, 0x14, 0x7C, 0x00, // 0x71 'q'
    0x7C, 0x08, 0x04, 0x04, 0x08, 0x00, // 0x72 'r'
    0x48, 0x54, 0x54, 0x54, 0x20, 0x00, // 0x73 's'
    0x04, 0x3F, 0x44, 0x40, 0x20, 0x00, // 0x74 't'
    0x3C, 0x40, 0x40, 0x20, 0x7C, 0x00, // 0x75 'u'
    0x1C, 0x20, 0x40, 0x20, 0x1C, 0x00, // 0x76 'v'
    0x7C, 0x20, 0x10, 0x20, 0x7C, 0x00, // 0x77 'w'
    0x44, 0x28, 0x10, 0x28, 0x44, 0x00, // 0x78 'x'
    0x0C, 0x50, 0x50, 0x50, 0x3C, 0x00, // 0x79 'y'
    0x44, 0x64, 0x54, 0x4C, 0x44, 0x00, // 0x7A 'z'
    0x00, 0x08, 0x36, 0x41, 0x00, 0x00, // 0x7B '{'
    0x00, 0x00, 0x7F, 0x00, 0x00, 0x00, // 0x7C '|'
    0x00, 0x41, 0x36, 0x08, 0x00, 0x00, // 0x7D '}'
    0x02, 0x01, 0x02, 0x04, 0x02, 0x00  // 0x7E '~'
};

//
//  -------------------- Funções de envio I2C (se a tela estiver presente) --------------------
//
static void ssd1306_write_cmd(uint8_t cmd)
{
    if (!s_screen_present)
        return;

    i2c_cmd_handle_t cmd_handle = i2c_cmd_link_create();
    i2c_master_start(cmd_handle);
    i2c_master_write_byte(cmd_handle, (SSD1306_ADDR << 1) | I2C_MASTER_WRITE, true);

    // 0x00 = enviando comando
    i2c_master_write_byte(cmd_handle, 0x00, true);
    i2c_master_write_byte(cmd_handle, cmd, true);

    i2c_master_stop(cmd_handle);
    esp_err_t ret = i2c_master_cmd_begin(I2C_PORT, cmd_handle, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd_handle);

    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Erro cmd 0x%02X: %s", cmd, esp_err_to_name(ret));
    }
}

static void ssd1306_write_data(uint8_t data)
{
    if (!s_screen_present)
        return;

    i2c_cmd_handle_t cmd_handle = i2c_cmd_link_create();
    i2c_master_start(cmd_handle);
    i2c_master_write_byte(cmd_handle, (SSD1306_ADDR << 1) | I2C_MASTER_WRITE, true);

    // 0x40 = enviando dados de display
    i2c_master_write_byte(cmd_handle, 0x40, true);
    i2c_master_write_byte(cmd_handle, data, true);

    i2c_master_stop(cmd_handle);
    esp_err_t ret = i2c_master_cmd_begin(I2C_PORT, cmd_handle, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd_handle);

    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Erro data 0x%02X: %s", data, esp_err_to_name(ret));
    }
}

//
//  -------------------- Inicialização I2C e detecção do SSD1306 --------------------
//
static esp_err_t i2c_master_init(void)
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_SDA_PIN,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = I2C_SCL_PIN,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_FREQ_HZ,
    };

    esp_err_t err = i2c_param_config(I2C_PORT, &conf);
    if (err != ESP_OK)
    {
        return err;
    }
    return i2c_driver_install(I2C_PORT, I2C_MODE_MASTER, 0, 0, 0);
}

/**
 * @brief Verifica se o SSD1306 responde no endereço 0x3C (ACK).
 */
static bool ssd1306_probe(void)
{
    i2c_cmd_handle_t cmd_handle = i2c_cmd_link_create();
    i2c_master_start(cmd_handle);
    i2c_master_write_byte(cmd_handle, (SSD1306_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_stop(cmd_handle);

    esp_err_t ret = i2c_master_cmd_begin(I2C_PORT, cmd_handle, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd_handle);

    return (ret == ESP_OK);
}

static void ssd1306_init_hw(void)
{
    // Sequência clássica de init do SSD1306 128x64
    ssd1306_write_cmd(0xAE); // OFF
    ssd1306_write_cmd(0x20);
    ssd1306_write_cmd(0x00); // Horizontal address
    ssd1306_write_cmd(0xB0);
    ssd1306_write_cmd(0xC8);
    ssd1306_write_cmd(0x00);
    ssd1306_write_cmd(0x10);
    ssd1306_write_cmd(0x40);
    ssd1306_write_cmd(0x81);
    ssd1306_write_cmd(0xFF); // contraste
    ssd1306_write_cmd(0xA1);
    ssd1306_write_cmd(0xA6);
    ssd1306_write_cmd(0xA8);
    ssd1306_write_cmd(0x3F);
    ssd1306_write_cmd(0xA4);
    ssd1306_write_cmd(0xD3);
    ssd1306_write_cmd(0x00);
    ssd1306_write_cmd(0xD5);
    ssd1306_write_cmd(0xF0);
    ssd1306_write_cmd(0xD9);
    ssd1306_write_cmd(0x22);
    ssd1306_write_cmd(0xDA);
    ssd1306_write_cmd(0x12);
    ssd1306_write_cmd(0xDB);
    ssd1306_write_cmd(0x20);
    ssd1306_write_cmd(0x8D);
    ssd1306_write_cmd(0x14);
    ssd1306_write_cmd(0xAF); // ON
}

//
//  -------------------- Implementações das funções da API screen.h --------------------
//
esp_err_t screen_init(void)
{
    esp_err_t err = i2c_master_init();
    if (err != ESP_OK)
    {
        return err;
    }

    // Tenta detectar o SSD1306
    bool found = ssd1306_probe();
    if (!found)
    {
        ESP_LOGW(TAG, "SSD1306 não encontrado no endereço 0x%02X. Continuando sem display...", SSD1306_ADDR);
        s_screen_present = false;
        // Retorna OK para não travar a aplicação
        return ESP_OK;
    }

    s_screen_present = true;
    ESP_LOGI(TAG, "SSD1306 detectado no endereço 0x%02X.", SSD1306_ADDR);

    // Limpa o buffer local
    memset(s_oled_buffer, 0, sizeof(s_oled_buffer));

    // Inicializa o hardware do SSD1306
    ssd1306_init_hw();

    return ESP_OK;
}

void screen_clear(void)
{
    // Fundo preto = bits em zero
    memset(s_oled_buffer, 0x00, sizeof(s_oled_buffer));
    if (s_screen_present)
    {
        screen_update_default();
    }
}

void screen_update_default(void)
{
    if (!s_screen_present)
    {
        return;
    }
    // O display tem 8 páginas (8 blocks de 128 bytes)
    for (uint8_t page = 0; page < OLED_PAGES; page++)
    {
        ssd1306_write_cmd(0xB0 + page); // page address
        ssd1306_write_cmd(0x00);        // lower col = 0
        ssd1306_write_cmd(0x10);        // higher col = 0

        for (uint8_t col = 0; col < OLED_WIDTH; col++)
        {
            ssd1306_write_data(s_oled_buffer[page * OLED_WIDTH + col]);
        }
    }
}

void screen_draw_string(int x, int y, const char *text)
{
    // Desenha texto “branco” em fundo preto (bit=1 liga o pixel)
    while (*text)
    {
        char c = *text;
        // Se fora do ASCII 0x20..0x7E, usa espaço
        if (c < 0x20 || c > 0x7E)
        {
            c = 0x20;
        }

        int font_index = (c - 0x20) * 6;

        // Para cada coluna (6 colunas), e cada bit (8 linhas)
        for (int col = 0; col < 6; col++)
        {
            uint8_t line_bits = ssd1306_font6x8[font_index + col];

            for (int row = 0; row < 8; row++)
            {
                int px = x + col;
                int py = y + row;
                if (px < 0 || px >= OLED_WIDTH || py < 0 || py >= OLED_HEIGHT)
                    continue;

                int page = py / 8;
                int bit_offset = py % 8;
                int index = page * OLED_WIDTH + px;

                bool pixel_on = (line_bits & (1 << row)) != 0;
                if (pixel_on)
                {
                    s_oled_buffer[index] |= (1 << bit_offset); // Liga bit
                }
                else
                {
                    s_oled_buffer[index] &= ~(1 << bit_offset); // Desliga
                }
            }
        }
        x += 6;
        text++;
    }
}

//
//  -------------------- Novas funções para fundo branco e texto preto --------------------
//

/**
 * @brief Limpa a tela deixando fundo branco (bits = 1).
 */
void screen_clear_default(void)
{
    memset(s_oled_buffer, 0xFF, sizeof(s_oled_buffer));
    if (s_screen_present)
    {
        screen_update_default();
    }
}

/**
 * @brief Desenha um caractere (fonte 6x8), porém escalado e “preto”
 *        (ou seja, limpa bit em vez de setar).
 *
 * @param x     Coordenada x na tela
 * @param y     Coordenada y na tela
 * @param c     Caractere a desenhar
 * @param scale Escala (1 = 6x8 normal, 2 = 12x16, etc.)
 */
static void screen_draw_char_scaled(int x, int y, char c, int scale)
{
    // Ajusta c para a faixa 0x20..0x7E
    if (c < 0x20 || c > 0x7E)
    {
        c = 0x20;
    }

    int font_index = (c - 0x20) * 6;

    for (int col = 0; col < 6; col++)
    {
        uint8_t bits = ssd1306_font6x8[font_index + col];

        for (int row = 0; row < 8; row++)
        {
            bool pixel_on = (bits & (1 << row)) != 0;
            if (pixel_on)
            {
                // Para “escala”, desenha um bloco scale×scale
                for (int sx = 0; sx < scale; sx++)
                {
                    for (int sy = 0; sy < scale; sy++)
                    {
                        int px = x + col * scale + sx;
                        int py = y + row * scale + sy;
                        if (px < 0 || px >= OLED_WIDTH || py < 0 || py >= OLED_HEIGHT)
                            continue;

                        int page = py / 8;
                        int bit_offset = py % 8;
                        int idx = page * OLED_WIDTH + px;
                        // Texto “branco” => limpar bit (fundo está 0)
                        s_oled_buffer[idx] |= (1 << bit_offset);
                    }
                }
            }
        }
    }
}

/**
 * @brief Desenha string inteira em preto, fundo branco, com escala.
 *
 * @param x     Coord. x do canto superior-esquerdo
 * @param y     Coord. y do canto superior-esquerdo
 * @param text  Texto a desenhar
 * @param scale Escala do caractere
 */
void screen_draw_string_scaled(int x, int y, const char *text, int scale)
{
    while (*text)
    {
        screen_draw_char_scaled(x, y, *text, scale);

        if (*text == ' ')
            x += (1 * scale); // espaço mais estreito
        else
            x += (5 * scale) + (scale / 2); // caractere normal com pequeno espaço

        text++;
    }
}

int screen_get_string_width(const char *text, int scale)
{
    int width = 0;

    while (*text)
    {
        if (*text == ' ')
            width += (1 * scale);
        else
            width += (5 * scale) + (scale / 2);

        text++;
    }

    return width;
}

