#include <string.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "led.h"       // Módulo LED
#include "screen.h"    // Módulo Tela OLED
#include "wifi.h"      // Nosso novo módulo Wi-Fi + ESP-NOW

static const char *TAG = "main";

// Tarefa que manda HELLO 10 vezes (como antes)
static void hello_task(void *pvParams)
{
    for (int i = 0; i < 10; i++)
    {
        ESP_LOGI(TAG, "Enviando HELLO %d/10", i + 1);
        // Agora chamamos a função do wifi.c
        wifi_send_hello();

        vTaskDelay(pdMS_TO_TICKS(3000));
    }
    ESP_LOGI(TAG, "Fim dos HELLOs");
    vTaskDelete(NULL);
}

void app_main(void)
{
    // Inicializa NVS
    ESP_ERROR_CHECK(nvs_flash_init());

    // Inicializa LED
    led_init();

    // Inicializa a tela
    ESP_ERROR_CHECK(screen_init());
    screen_clear();
    screen_draw_string(0, 0, "AR CONDICIONADO");
    screen_draw_string(0, 8, "Aguardando comando");
    screen_update();

    // Inicializa Wi-Fi + ESP-NOW via nosso módulo wifi.c
    wifi_init_all();

    // Cria tarefa que manda HELLO
    xTaskCreate(hello_task, "hello_task", 2048, NULL, 5, NULL);

    // Loop principal
    while (1)
    {
        // Alterna LED
        led_toggle();

        // Pergunta ao led se está ON e envia "DATA" aos peers
        bool led_on = led_get_state();
        wifi_send_data_to_peers(led_on);

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
