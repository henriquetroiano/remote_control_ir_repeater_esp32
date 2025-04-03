#include <string.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "commander.h" // 👈 adiciona

#include "led.h"
#include "screen.h"
#include "wifi.h"
#include "write.h"
#include "keyboard.h" // 👈 Adicionado para usar discover

static const char *TAG = "main";

// Tarefa que manda HELLO 10 vezes
static void hello_task(void *pvParams)
{
    for (int i = 0; i < 10; i++)
    {
        ESP_LOGI(TAG, "Enviando HELLO %d/10", i + 1);
        wifi_send_hello();
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
    ESP_LOGI(TAG, "Fim dos HELLOs");
    vTaskDelete(NULL);
}

void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());

    led_init();
    ESP_ERROR_CHECK(screen_init());
    write_default("TODOS");
    wifi_init_all();

    // Inicia a tarefa de descoberta do teclado
    keyboard_init();
    commander_init(); // 👈 adiciona aqui
    xTaskCreate(keyboard_task, "keyboard_task", 2048, NULL, 5, NULL);

    xTaskCreate(hello_task, "hello_task", 4096, NULL, 5, NULL);

    while (1)
    {
        led_toggle();

        bool led_on = led_get_state();
        wifi_send_data_to_peers(led_on);

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
