#include <stdio.h>
#include <string.h>
#include "driver/rmt_tx.h"
#include "driver/rmt_rx.h"
#include "esp_log.h"
#include "infrared.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_timer.h"
#include "wifi.h"

#define RMT_RX_GPIO 15
#define RMT_TX_GPIO 23
#define RMT_CLK_HZ 1000000
#define RMT_MEM_BLOCK_SYMBOLS 64
#define RMT_RESOLUTION_HZ 1000000
#define MAX_PULSES 1000
#define MAX_SYMBOLS 256

typedef struct {
    uint32_t pulse_data[MAX_PULSES];
    int length;
} ir_raw_signal_t;

static const char *TAG = "IR_RMT";

static rmt_channel_handle_t rmt_tx_channel = NULL;
static rmt_channel_handle_t rmt_rx_channel = NULL;
static rmt_encoder_handle_t rmt_encoder = NULL;
static QueueHandle_t ir_send_queue = NULL;
static rmt_symbol_word_t ir_rx_buffer[MAX_SYMBOLS];

static bool rmt_rx_callback(rmt_channel_handle_t channel, const rmt_rx_done_event_data_t *edata, void *user_data)
{
    if (edata && edata->num_symbols > 0) {
        if (edata->num_symbols > MAX_PULSES) {
            ESP_LOGW(TAG, "Número de símbolos excede o limite (%d > %d)", edata->num_symbols, MAX_PULSES);
            return false;
        }

        ir_raw_signal_t signal;
        memset(&signal, 0, sizeof(signal));

        for (int i = 0; i < edata->num_symbols; i++) {
            signal.pulse_data[i] = edata->received_symbols[i].duration0;
        }
        signal.length = edata->num_symbols;

        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        xQueueSendFromISR(ir_send_queue, &signal, &xHigherPriorityTaskWoken);

        return xHigherPriorityTaskWoken == pdTRUE;
    }
    return false;
}

void infrared_init(void)
{
    ESP_LOGI(TAG, "Inicializando RMT para TX e RX...");

    rmt_tx_channel_config_t tx_config = {
        .gpio_num = RMT_TX_GPIO,
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .mem_block_symbols = RMT_MEM_BLOCK_SYMBOLS,
        .resolution_hz = RMT_RESOLUTION_HZ,
        .trans_queue_depth = 4,
        .flags = {
            .invert_out = false
        }
    };
    ESP_ERROR_CHECK(rmt_new_tx_channel(&tx_config, &rmt_tx_channel));
    ESP_ERROR_CHECK(rmt_enable(rmt_tx_channel));

    rmt_copy_encoder_config_t encoder_config = {};
    ESP_ERROR_CHECK(rmt_new_copy_encoder(&encoder_config, &rmt_encoder));

    rmt_rx_channel_config_t rx_config = {
        .gpio_num = RMT_RX_GPIO,
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .mem_block_symbols = RMT_MEM_BLOCK_SYMBOLS,
        .resolution_hz = RMT_RESOLUTION_HZ,
        .flags = {
            .with_dma = false
        }
    };
    ESP_ERROR_CHECK(rmt_new_rx_channel(&rx_config, &rmt_rx_channel));

    rmt_rx_event_callbacks_t callbacks = {
        .on_recv_done = rmt_rx_callback
    };
    ESP_ERROR_CHECK(rmt_rx_register_event_callbacks(rmt_rx_channel, &callbacks, NULL));
    ESP_ERROR_CHECK(rmt_enable(rmt_rx_channel));

    ir_send_queue = xQueueCreate(4, sizeof(ir_raw_signal_t));
    assert(ir_send_queue != NULL);

    rmt_receive_config_t recv_cfg = {
        .signal_range_min_ns = 500,
        .signal_range_max_ns = 15000000
    };

    esp_err_t err = rmt_receive(rmt_rx_channel, ir_rx_buffer, sizeof(ir_rx_buffer), &recv_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Erro em rmt_receive: 0x%x", err);
        return;
    }

    ESP_LOGI(TAG, "Infrared RMT initialized. TX: GPIO %d | RX: GPIO %d", RMT_TX_GPIO, RMT_RX_GPIO);
}

void infrared_send(const rmt_symbol_word_t *symbols, size_t symbol_num)
{
    if (!symbols || symbol_num == 0) {
        ESP_LOGW(TAG, "Tentativa de envio com buffer vazio.");
        return;
    }

    rmt_transmit_config_t tx_config = {
        .loop_count = 0,
        .flags = {
            .eot_level = 0
        }
    };

    ESP_ERROR_CHECK(rmt_transmit(rmt_tx_channel, rmt_encoder, symbols, symbol_num * sizeof(rmt_symbol_word_t), &tx_config));
    ESP_ERROR_CHECK(rmt_tx_wait_all_done(rmt_tx_channel, -1));
    ESP_LOGI(TAG, "Sinal IR transmitido (%zu símbolos).", symbol_num);
}

void infrared_task(void *arg)
{
    ir_raw_signal_t signal;

    while (1) {
        memset(&signal, 0, sizeof(signal));

        if (xQueueReceive(ir_send_queue, &signal, portMAX_DELAY)) {
            if (signal.length <= 0 || signal.length > MAX_PULSES) {
                ESP_LOGW(TAG, "Sinal IR com tamanho inválido recebido da fila: %d", signal.length);
                continue;
            }

            // Segurança adicional: evita crash no printf mesmo se o struct tiver sido corrompido
            uint32_t safe_length = signal.length;

            bool valid = true;
            for (int i = 0; i < safe_length; i++) {
                if (signal.pulse_data[i] > 10000000) {
                    ESP_LOGW(TAG, "Pulso fora do intervalo: %lu (i=%d)", (unsigned long)signal.pulse_data[i], i);
                    valid = false;
                    break;
                }
            }

            if (!valid) {
                continue;
            }

            ESP_LOGI(TAG, "Recebido sinal IR com %d pulsos", safe_length);

            int display_count = safe_length < 20 ? safe_length : 20;
            for (int i = 0; i < display_count; i++) {
                ESP_LOGI(TAG, "Pulso[%d]: %lu", i, (unsigned long)signal.pulse_data[i]);
            }
            if (safe_length > 20) {
                ESP_LOGI(TAG, "... e mais %d pulsos", safe_length - 20);
            }

            wifi_send_ir("TODOS", signal.pulse_data, safe_length);
        }
    }
}


void ir_send_raw(const uint32_t *pulses, int len)
{
    if (!pulses || len <= 0 || len > MAX_PULSES || !ir_send_queue) {
        ESP_LOGW(TAG, "ir_send_raw chamado com parâmetros inválidos.");
        return;
    }

    ir_raw_signal_t signal;
    memset(&signal, 0, sizeof(signal));

    memcpy(signal.pulse_data, pulses, len * sizeof(uint32_t));
    signal.length = len;

    if (xQueueSend(ir_send_queue, &signal, 0) != pdPASS) {
        ESP_LOGW(TAG, "Fila cheia ou erro ao enviar sinal IR.");
    }
}
