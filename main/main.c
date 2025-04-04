#include <string.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "commander.h"
#include "led.h"
#include "screen.h"
#include "wifi.h"
#include "write.h"
#include "keyboard.h"
#include "infrared.h" // Agora adaptado para RMT!

static const char *TAG = "main";

// Exemplo de tarefa qualquer
static void hello_task(void *pvParams)
{
    for (int i = 0; i < 10; i++)
    {
        wifi_send_hello();
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
    vTaskDelete(NULL);
}

void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());

    led_init();
    ESP_ERROR_CHECK(screen_init());
    write_default("TODOS");
    wifi_init_all();

    keyboard_init();
    commander_init();

    // --- Inicializa Infrared (com RMT) ---
    infrared_init();

    // Cria tarefas
    xTaskCreate(keyboard_task, "keyboard_task", 2048, NULL, 5, NULL);
    xTaskCreate(hello_task, "hello_task", 4096, NULL, 5, NULL);

    // Tarefas IR
    xTaskCreate(infrared_task, "infrared_task", 32768, NULL, 5, NULL);
    xTaskCreate(ir_send_task, "ir_send_task", 32768, NULL, 4, NULL);

    while (1)
    {
        led_toggle();
        bool led_on = led_get_state();
        vTaskDelay(pdMS_TO_TICKS(1000));

        // Se desejar, mande broadcast do estado do LED
        // wifi_send_data_to_peers(led_on);
        // ESP_LOGI(TAG, "Memória livre: %d bytes", esp_get_free_heap_size());

    }
}
