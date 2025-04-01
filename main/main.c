#include <string.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_now.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_mac.h"

#define GROUP_CODE "GRUPO_X"
#define MAX_PEERS 20
#define BLINK_GPIO CONFIG_BLINK_GPIO
#define BLINK_PERIOD 1000

static const char *TAG = "espnow_debug";
static uint8_t s_led_state = 0;

typedef struct
{
    char type[8];
    char code[16];
    char data[32];
} __attribute__((packed)) message_t;

esp_now_peer_info_t peers[MAX_PEERS];
int peer_count = 0;

void on_data_recv(const esp_now_recv_info_t *recv_info, const uint8_t *data, int data_len);

void blink_led(void)
{
    gpio_set_level(BLINK_GPIO, s_led_state);
}

void configure_led(void)
{
    gpio_reset_pin(BLINK_GPIO);
    gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);
}

void print_mac(const char *prefix, const uint8_t *mac)
{
    ESP_LOGI(TAG, "%s %02X:%02X:%02X:%02X:%02X:%02X",
             prefix, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

void add_peer_if_needed(const uint8_t *mac)
{
    for (int i = 0; i < peer_count; ++i)
    {
        if (memcmp(peers[i].peer_addr, mac, 6) == 0)
        {
            ESP_LOGI(TAG, "Peer já registrado.");
            return;
        }
    }

    if (peer_count < MAX_PEERS)
    {
        esp_now_peer_info_t peer = {
            .channel = 0,
            .ifidx = WIFI_IF_STA,
            .encrypt = false,
        };
        memcpy(peer.peer_addr, mac, 6);
        esp_err_t err = esp_now_add_peer(&peer);
        if (err == ESP_OK)
        {
            peers[peer_count++] = peer;
            print_mac("✅ Novo peer registrado:", mac);
        }
        else
        {
            ESP_LOGE(TAG, "Erro ao registrar peer: %s", esp_err_to_name(err));
        }
    }
    else
    {
        ESP_LOGW(TAG, "⚠️ Limite de peers atingido!");
    }
}

void send_hello()
{
    uint8_t broadcast_mac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    message_t msg = {0};
    strcpy(msg.type, "HELLO");
    strcpy(msg.code, GROUP_CODE);
    esp_err_t err = esp_now_send(broadcast_mac, (uint8_t *)&msg, sizeof(msg));
    if (err == ESP_OK)
    {
        ESP_LOGI(TAG, "📢 HELLO enviado via broadcast.");
    }
    else
    {
        ESP_LOGE(TAG, "❌ Falha ao enviar HELLO: %s", esp_err_to_name(err));
    }
}

void send_data_to_peers()
{
    message_t msg = {0};
    strcpy(msg.type, "DATA");
    strcpy(msg.code, GROUP_CODE);
    sprintf(msg.data, "LED %s", s_led_state ? "ON" : "OFF");

    for (int i = 0; i < peer_count; ++i)
    {
        print_mac("📤 Enviando DATA para", peers[i].peer_addr);
        esp_err_t err = esp_now_send(peers[i].peer_addr, (uint8_t *)&msg, sizeof(msg));
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "❌ Falha ao enviar para peer %d: %s", i, esp_err_to_name(err));
        }
    }
}

void send_welcome(const uint8_t *dest_mac)
{
    message_t msg = {0};
    strcpy(msg.type, "WELCOME");
    strcpy(msg.code, GROUP_CODE);
    sprintf(msg.data, "MAC OK");
    esp_err_t err = esp_now_send(dest_mac, (uint8_t *)&msg, sizeof(msg));
    print_mac("📤 Respondendo WELCOME para", dest_mac);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "❌ Falha ao enviar WELCOME: %s", esp_err_to_name(err));
    }
}

void on_data_recv(const esp_now_recv_info_t *recv_info, const uint8_t *data, int data_len)
{
    if (data_len != sizeof(message_t))
    {
        ESP_LOGW(TAG, "⚠️ Mensagem com tamanho inválido: %d", data_len);
        return;
    }

    const uint8_t *mac = recv_info->src_addr;
    message_t *msg = (message_t *)data;

    // Obter MAC local
    uint8_t my_mac[6];
    esp_read_mac(my_mac, ESP_MAC_WIFI_STA);

    if (memcmp(mac, my_mac, 6) == 0)
    {
        ESP_LOGW(TAG, "⚠️ Ignorando mensagem recebida do próprio MAC.");
        return;
    }

    print_mac(msg->type, mac);
    ESP_LOGI(TAG, "📨 Conteúdo: '%s'", msg->data);

    if (strcmp(msg->code, GROUP_CODE) != 0)
    {
        ESP_LOGW(TAG, "⚠️ Código do grupo inválido: %s", msg->code);
        return;
    }

    if (strcmp(msg->type, "HELLO") == 0)
    {
        add_peer_if_needed(mac); // ✅ Corrigido aqui!
        send_welcome(mac);
    }
    else if (strcmp(msg->type, "WELCOME") == 0)
    {
        add_peer_if_needed(mac);
    }
    else if (strcmp(msg->type, "DATA") == 0)
    {
        add_peer_if_needed(mac); // ✅ Corrigido aqui!
        // ação customizada aqui
    }
}

void on_data_sent(const uint8_t *mac_addr, esp_now_send_status_t status)
{
    print_mac("📤 Envio concluído para", mac_addr);
    ESP_LOGI(TAG, "📦 Status: %s", status == ESP_NOW_SEND_SUCCESS ? "SUCESSO" : "FALHA");
}

void init_wifi()
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM)); // ✅ evita salvar config
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_ERROR_CHECK(esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE)); // ✅ necessário
    ESP_ERROR_CHECK(esp_wifi_set_protocol(WIFI_IF_STA,
                                          WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N));
}

void init_espnow()
{
    if (esp_now_is_peer_exist((uint8_t[]){0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}))
    {
        ESP_ERROR_CHECK(esp_now_deinit()); // ✅ segura caso já tenha sido iniciado
    }

    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_recv_cb(on_data_recv));
    ESP_ERROR_CHECK(esp_now_register_send_cb(on_data_sent));

    const uint8_t bcast[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

    esp_now_peer_info_t peer = {
        .channel = 1, // ✅ use mesmo canal setado no Wi-Fi
        .ifidx = WIFI_IF_STA,
        .encrypt = false};
    memcpy(peer.peer_addr, bcast, 6);

    if (!esp_now_is_peer_exist(bcast))
    {
        esp_err_t err = esp_now_add_peer(&peer);
        if (err == ESP_OK)
        {
            ESP_LOGI(TAG, "📡 Peer de broadcast registrado.");
        }
        else
        {
            ESP_LOGE(TAG, "❌ Erro ao registrar peer de broadcast: %s", esp_err_to_name(err));
        }
    }
}

void hello_task(void *pvParams)
{
    for (int i = 0; i < 10; i++)
    {
        ESP_LOGI(TAG, "⏱️  Enviando HELLO (%d/10)", i + 1);
        send_hello();
        vTaskDelay(3000 / portTICK_PERIOD_MS);
    }
    ESP_LOGI(TAG, "✅ Fim do envio de HELLOs");
    vTaskDelete(NULL);
}

void wait_for_wifi_ready()
{
    wifi_mode_t mode;
    wifi_interface_t iface;
    wifi_second_chan_t second;
    uint8_t channel = 0;
    esp_err_t err;

    for (int retry = 0; retry < 20; retry++)
    {
        err = esp_wifi_get_channel(&channel, &second);
        if (err == ESP_OK && channel > 0)
        {
            ESP_LOGI(TAG, "✅ Wi-Fi ativo no canal %d", channel);
            return;
        }
        ESP_LOGW(TAG, "⏳ Aguardando Wi-Fi estabilizar... (%d)", retry + 1);
        vTaskDelay(200 / portTICK_PERIOD_MS);
    }

    ESP_LOGE(TAG, "❌ Timeout esperando Wi-Fi pronto");
}

void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    configure_led();
    init_wifi();

    wait_for_wifi_ready(); // ✅ Garante que o Wi-Fi realmente iniciou

    init_espnow();

    // ⏳ Pequeno delay para garantir que Wi-Fi e ESP-NOW estejam prontos
    vTaskDelay(1000 / portTICK_PERIOD_MS); // 1000 ms de paz

    // Log do MAC local
    uint8_t mymac[6];
    esp_read_mac(mymac, ESP_MAC_WIFI_STA);
    print_mac("📍 Meu MAC é:", mymac);

    // Inicia tarefa de envio de HELLOs
    xTaskCreate(hello_task, "hello_task", 2048, NULL, 5, NULL);

    while (1)
    {
        blink_led();
        send_data_to_peers();
        s_led_state = !s_led_state;
        vTaskDelay(BLINK_PERIOD / portTICK_PERIOD_MS);
    }
}
