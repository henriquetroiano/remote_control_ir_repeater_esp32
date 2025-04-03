#include "keyboard.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "commander.h"  // 👈 adiciona no topo

#define TAG "KEYBOARD"
#define NUM_ROWS 4
#define NUM_COLS 4

// GPIOs conectados ao teclado
static const gpio_num_t row_pins[NUM_ROWS] = {
    GPIO_NUM_19, GPIO_NUM_18, GPIO_NUM_5, GPIO_NUM_17
};

static const gpio_num_t col_pins[NUM_COLS] = {
    GPIO_NUM_16, GPIO_NUM_4, GPIO_NUM_2, GPIO_NUM_32  // ← substituído aqui
};

// Mapeamento do teclado 4x4
static const char keymap[NUM_ROWS][NUM_COLS] = {
    {'1', '2', '3', 'A'},
    {'4', '5', '6', 'B'},
    {'7', '8', '9', 'C'},
    {'*', '0', '#', 'D'}
};

void keyboard_init(void)
{
    for (int i = 0; i < NUM_COLS; i++) {
        gpio_set_direction(col_pins[i], GPIO_MODE_OUTPUT);
        gpio_set_level(col_pins[i], 1);
    }

    for (int i = 0; i < NUM_ROWS; i++) {
        gpio_set_direction(row_pins[i], GPIO_MODE_INPUT);
        gpio_set_pull_mode(row_pins[i], GPIO_PULLUP_ONLY);
    }

    ESP_LOGI(TAG, "Teclado inicializado.");
}

char keyboard_scan(void)
{
    for (int col = 0; col < NUM_COLS; col++) {
        gpio_set_level(col_pins[col], 0);
        vTaskDelay(pdMS_TO_TICKS(1));

        for (int row = 0; row < NUM_ROWS; row++) {
            int val = gpio_get_level(row_pins[row]);
            if (val == 0) {
                ESP_LOGI(TAG, "Detectado em col=%d, row=%d", col, row);
                while (gpio_get_level(row_pins[row]) == 0) {
                    vTaskDelay(pdMS_TO_TICKS(10));
                }
                gpio_set_level(col_pins[col], 1);
                return keymap[row][col];
            }
        }

        gpio_set_level(col_pins[col], 1);
    }

    return 0;
}

void keyboard_task(void *pvParameters)
{
    while (1) {
        char key = keyboard_scan();
        if (key != 0) {
            ESP_LOGI(TAG, "Tecla pressionada: %c", key);
            commander_process_key(key);  // 👈 envia tecla para commander
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}
