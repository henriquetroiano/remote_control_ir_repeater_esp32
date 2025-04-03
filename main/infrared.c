#include "infrared.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <string.h>

#include "wifi.h"

#define IR_RECV_GPIO GPIO_NUM_15
#define IR_SEND_GPIO GPIO_NUM_23
#define MAX_PULSES 200

static const char *TAG = "IR_RAW";

// Estrutura para passar dados via fila
typedef struct
{
    uint32_t pulse_data[MAX_PULSES];
    int length;
} ir_raw_signal_t;

static uint32_t pulses[MAX_PULSES];
static int pulse_count = 0;
static int64_t last_time = 0;

static QueueHandle_t ir_send_queue;

// === PWM para modulação de 38kHz ===

void infrared_pwm_init()
{
    ledc_timer_config_t ledc_timer = {
        .speed_mode = LEDC_HIGH_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_8_BIT,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = 38000,  // 38 kHz
        .clk_cfg = LEDC_AUTO_CLK
    };
    ledc_timer_config(&ledc_timer);

    ledc_channel_config_t ledc_channel = {
        .speed_mode = LEDC_HIGH_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .timer_sel = LEDC_TIMER_0,
        .intr_type = LEDC_INTR_DISABLE,
        .gpio_num = IR_SEND_GPIO,
        .duty = 0,  // Começa desligado
        .hpoint = 0
    };
    ledc_channel_config(&ledc_channel);
}

void ir_carrier_on()
{
    ledc_set_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0, 128);  // 50% duty (128/255)
    ledc_update_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0);
}

void ir_carrier_off()
{
    ledc_set_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0, 0);
    ledc_update_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0);
}

// === Fim do controle PWM ===

void IRAM_ATTR gpio_isr_handler(void *arg)
{
    int level = gpio_get_level(IR_RECV_GPIO);
    int64_t now = esp_timer_get_time();
    int64_t duration = now - last_time;

    if (pulse_count < MAX_PULSES)
        pulses[pulse_count++] = (uint32_t)duration;

    last_time = now;
}

void infrared_init()
{
    // Config receptor
    gpio_config_t rx_conf = {
        .pin_bit_mask = (1ULL << IR_RECV_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_ANYEDGE};
    gpio_config(&rx_conf);

    gpio_install_isr_service(0);
    gpio_isr_handler_add(IR_RECV_GPIO, gpio_isr_handler, NULL);

    // Config transmissor (GPIO será controlado pelo LEDC)
    gpio_config_t tx_conf = {
        .pin_bit_mask = (1ULL << IR_SEND_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE};
    gpio_config(&tx_conf);

    infrared_pwm_init(); // Inicializa modulação PWM

    // Cria fila
    ir_send_queue = xQueueCreate(4, sizeof(ir_raw_signal_t));

    ESP_LOGI(TAG, "Infrared initialized. RX: GPIO %d, TX: GPIO %d (PWM 38kHz)", IR_RECV_GPIO, IR_SEND_GPIO);
}

// 🔁 Task separada para envio IR
void ir_send_task(void *arg)
{
    ir_raw_signal_t signal;

    while (1)
    {
        if (xQueueReceive(ir_send_queue, &signal, portMAX_DELAY))
        {
            ESP_LOGI(TAG, "Sending IR with %d pulses", signal.length);

            for (int i = 0; i < signal.length; i++)
            {
                if (i % 2 == 0)
                    ir_carrier_on();  // Envia pulso com PWM
                else
                    ir_carrier_off(); // Pausa (sem PWM)

                esp_rom_delay_us(signal.pulse_data[i]);

                if (i % 20 == 0)
                    taskYIELD(); // evitar WDT
            }

            ir_carrier_off(); // Garante desligado no final
            ESP_LOGI(TAG, "IR signal sent.");
        }
    }
}

// 🔍 Task principal: escuta IR e envia para fila
void infrared_task(void *arg)
{
    while (1)
    {
        if (pulse_count > 0 && esp_timer_get_time() - last_time > 50000)
        {
            ir_raw_signal_t signal;
            int start = (pulses[0] > 50000) ? 1 : 0;
            int len = pulse_count - start;

            if (len > MAX_PULSES)
                len = MAX_PULSES;

            for (int i = 0; i < len; i++)
                signal.pulse_data[i] = pulses[i + start];
            signal.length = len;

            ESP_LOGI(TAG, "Received IR with %d pulses", len);
            for (int i = 0; i < len; i++)
                printf("%lu%s", (unsigned long)signal.pulse_data[i], (i < len - 1) ? "," : "\n");

            xQueueSend(ir_send_queue, &signal, 0);
            wifi_send_ir("TODOS", signal.pulse_data, signal.length);
            pulse_count = 0;
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void ir_send_raw(const uint32_t *pulses, int len)
{
    if (len <= 0 || len > MAX_PULSES)
        return;

    ir_raw_signal_t signal = {0};
    memcpy(signal.pulse_data, pulses, len * sizeof(uint32_t));
    signal.length = len;

    xQueueSend(ir_send_queue, &signal, 0);
}
