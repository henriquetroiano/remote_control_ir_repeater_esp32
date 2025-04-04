#include "infrared.h"
#include <string.h>
#include <stdio.h>

#include "esp_log.h"
#include "esp_timer.h"
#include "esp_rom_sys.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "driver/rmt.h"
#include "driver/gpio.h"

#include "wifi.h"

static const char *TAG = "IR_RMT";

// Pinos
#define IR_RECV_GPIO GPIO_NUM_15
#define IR_SEND_GPIO GPIO_NUM_23

// Limites
#define MAX_PULSES   200

// Fila local para "enviar IR"
typedef struct {
    uint32_t pulse_data[MAX_PULSES];
    int length;
} ir_raw_signal_t;

static QueueHandle_t ir_send_queue = NULL;

// -----------------------------------
// Inicialização dos dois canais RMT
// -----------------------------------
static void rmt_rx_init(void)
{
    rmt_config_t rmt_rx = {
        .rmt_mode = RMT_MODE_RX,
        .channel = RMT_CHANNEL_1,
        .gpio_num = IR_RECV_GPIO,
        // Divisor de clock 80 => 1 tick = 1us (com APB a 80 MHz)
        .clk_div = 80,
        .mem_block_num = 4, // Mais blocos se o sinal for longo
        .rx_config = {
            .filter_en = true,
            // Filtra ruídos menores que 100us, por exemplo
            .filter_ticks_thresh = 100,
            // tempo máximo sem borda antes de considerar que o sinal acabou
            .idle_threshold = 60000 // 60 ms
        }
    };
    ESP_ERROR_CHECK(rmt_config(&rmt_rx));
    ESP_ERROR_CHECK(rmt_driver_install(rmt_rx.channel, 1000 /*tamanho rb*/, 0));
    ESP_LOGI(TAG, "RMT RX inicializado no canal %d (GPIO %d)", RMT_CHANNEL_1, IR_RECV_GPIO);
}

static void rmt_tx_init(void)
{
    rmt_config_t rmt_tx = {
        .rmt_mode = RMT_MODE_TX,
        .channel = RMT_CHANNEL_0,
        .gpio_num = IR_SEND_GPIO,
        // 1 tick = 1us se clk_div=80, APB = 80MHz
        .clk_div = 80,
        .mem_block_num = 1,
        .tx_config = {
            // Usa carrier interna
            .carrier_en = true,
            .carrier_freq_hz = 38000,
            .carrier_level = RMT_CARRIER_LEVEL_HIGH,
            .carrier_duty_percent = 50,
            .loop_en = false,
            .idle_output_en = true,
            .idle_level = RMT_IDLE_LEVEL_LOW,
        }
    };
    ESP_ERROR_CHECK(rmt_config(&rmt_tx));
    ESP_ERROR_CHECK(rmt_driver_install(rmt_tx.channel, 0, 0));
    ESP_LOGI(TAG, "RMT TX inicializado no canal %d (GPIO %d)", RMT_CHANNEL_0, IR_SEND_GPIO);
}

// -----------------------------------
// Funções públicas
// -----------------------------------
void infrared_init(void)
{
    // Inicializa canal de recepção
    rmt_rx_init();

    // Inicializa canal de transmissão
    rmt_tx_init();

    // Cria a fila para transmissão local
    ir_send_queue = xQueueCreate(4, sizeof(ir_raw_signal_t));

    ESP_LOGI(TAG, "Infrared (RMT) initialized. RX: GPIO %d, TX: GPIO %d", IR_RECV_GPIO, IR_SEND_GPIO);
}

// Esta função coloca na fila local e a tarefa "ir_send_task" fará o rmt_write_items
void ir_send_raw(const uint32_t *pulses, int len)
{
    if (len <= 0 || len > MAX_PULSES) {
        ESP_LOGW(TAG, "Tamanho de pulso inválido: %d", len);
        return;
    }

    ir_raw_signal_t signal = {0};
    memcpy(signal.pulse_data, pulses, len * sizeof(uint32_t));
    signal.length = len;

    // Envia para a fila, sem bloqueio
    if (xQueueSend(ir_send_queue, &signal, 0) != pdTRUE) {
        ESP_LOGW(TAG, "Fila de IR cheia, sinal foi descartado!");
    }
}

// -----------------------------------
// Tarefa de TX: Lê da fila e envia via RMT
// -----------------------------------
void ir_send_task(void *arg)
{
    ir_raw_signal_t signal;

    while (1) {
        if (xQueueReceive(ir_send_queue, &signal, portMAX_DELAY)) {
            ESP_LOGI(TAG, "Enviando IR (RMT) com %d pulsos", signal.length);

            // Precisamos converter a lista de pulsos em items RMT
            // Cada "pulso" no seu código raw costuma ser: [tempoNivelAlto, tempoNivelBaixo, tempoNivelAlto, ...]
            // Aqui vamos assumir que signal.pulse_data alterna High e Low.
            // Exemplo: se o array for [9000,4500,560,560,560,560,...]
            // Precisamos "agrupar" a cada 2 valores em um item:
            //   item.level0 = 1;
            //   item.duration0 = 9000us
            //   item.level1 = 0;
            //   item.duration1 = 4500us
            // e assim por diante.

            int item_count = signal.length / 2;
            if (signal.length % 2 != 0) {
                // Se for ímpar, descartamos o último ou fazemos outra estratégia
                item_count = signal.length / 2; // trunca
            }

            rmt_item32_t *items = calloc(item_count, sizeof(rmt_item32_t));
            if (!items) {
                ESP_LOGE(TAG, "Falha ao alocar memória para RMT items");
                continue;
            }

            for (int i = 0; i < item_count; i++) {
                uint32_t high_us = signal.pulse_data[2*i];
                uint32_t low_us  = signal.pulse_data[2*i + 1];

                // Level alto primeiro
                items[i].level0 = 1;  // HIGH
                items[i].duration0 = (high_us);  // em ticks de 1us
                // Depois level baixo
                items[i].level1 = 0;  // LOW
                items[i].duration1 = (low_us);   // em ticks de 1us
            }

            // Envia via RMT
            esp_err_t err = rmt_write_items(RMT_CHANNEL_0, items, item_count, true /*wait finish*/);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "Erro ao enviar items RMT: %s", esp_err_to_name(err));
            }
            // Libera
            free(items);

            // Aguarda terminar
            rmt_wait_tx_done(RMT_CHANNEL_0, pdMS_TO_TICKS(200));
            ESP_LOGI(TAG, "IR (RMT) enviado.");
        }
    }
}

// -----------------------------------
// Tarefa de RX: Lê do ring buffer RMT e detecta sinal
// -----------------------------------
void infrared_task(void *arg)
{
    // Recebemos as amostras do ringbuffer
    RingbufHandle_t rb = NULL;
    // Atribui ring buffer do canal RMT
    rmt_get_ringbuf_handle(RMT_CHANNEL_1, &rb);
    // Inicia recepção
    rmt_rx_start(RMT_CHANNEL_1, true);

    while (1) {
        size_t item_size = 0;
        // items: array de rmt_item32_t
        rmt_item32_t *items = (rmt_item32_t *) xRingbufferReceive(rb, &item_size, pdMS_TO_TICKS(1000));
        if (items) {
            // item_size é o total de bytes
            int n_items = item_size / sizeof(rmt_item32_t);
            ESP_LOGI(TAG, "Recebido IR (RMT) com %d items", n_items);

            // Converte de items RMT de volta para array de pulsos (high, low, high, low,...)
            uint32_t pulses[MAX_PULSES];
            int pulse_count = 0;

            for (int i = 0; i < n_items; i++) {
                uint32_t high_us = items[i].duration0; // pois 1 tick = 1us
                uint32_t low_us  = items[i].duration1;
                // Armazena no array
                if (pulse_count + 2 < MAX_PULSES) {
                    pulses[pulse_count++] = high_us;
                    pulses[pulse_count++] = low_us;
                } else {
                    ESP_LOGW(TAG, "Estourou limite de pulses. Truncando...");
                    break;
                }
            }

            // Liberar buffer do ring
            vRingbufferReturnItem(rb, (void *) items);

            ESP_LOGI(TAG, "Convertido para %d pulsos no total", pulse_count);
            // Print de debug
            /*
            for (int k = 0; k < pulse_count; k++) {
                printf("%u%s", pulses[k], (k < pulse_count - 1) ? "," : "\n");
            }
            */

            // Reenviar esse IR para TODOS via Wi-Fi (como já fazia antes)
            wifi_send_ir("TODOS", pulses, pulse_count);
        } else {
            // Timeout
            // Se quiser, faça algo aqui (ex. print debug)
        }
    }
}

// Fim de "infrared.c"
