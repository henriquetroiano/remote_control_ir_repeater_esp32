#include "wifi.h"
#include <string.h>
#include <stdio.h>
#include "esp_log.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "esp_now.h"
#include "esp_err.h"
#include "esp_mac.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "led.h"
#include "infrared.h"

#define GROUP_CODE "GRUPO_X"
#define MAX_PEERS 20
#define MAX_PULSES 200

static const char *TAG = "wifi_module";

// Estrutura da mensagem padrão
typedef struct
{
    char type[8];
    char code[16];
    char data[32];
} __attribute__((packed)) message_t;

// Estrutura para envio de sinal IR
typedef struct
{
    char id[32];                     
    int pulse_len;                   
    uint32_t pulse_data[MAX_PULSES]; 
} __attribute__((packed)) ir_message_t;

static esp_now_peer_info_t peers[MAX_PEERS];
static int peer_count = 0;

// ------------------- Protótipos internos -------------------
static void init_wifi(void);
static void wait_for_wifi_ready(void);
static void init_espnow(void);
static void add_peer_if_needed(const uint8_t *mac);
static void send_welcome(const uint8_t *dest_mac);
static void on_data_recv(const esp_now_recv_info_t *recv_info, const uint8_t *data, int data_len);
static void on_data_sent(const uint8_t *mac_addr, esp_now_send_status_t status);
static void print_mac(const char *prefix, const uint8_t *mac);

// ------------------- Funções públicas -------------------

void wifi_init_all(void)
{
    init_wifi();
    wait_for_wifi_ready();
    init_espnow();
}

void wifi_send_hello(void)
{
    uint8_t bcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    message_t msg = {0};
    strcpy(msg.type, "HELLO");
    strcpy(msg.code, GROUP_CODE);

    esp_err_t err = esp_now_send(bcast, (uint8_t *)&msg, sizeof(msg));
    if (err == ESP_OK)
        ESP_LOGI(TAG, "HELLO enviado (broadcast).");
    else
        ESP_LOGE(TAG, "Falha ao enviar HELLO: %s", esp_err_to_name(err));
}

void wifi_send_data_to_peers(bool led_on)
{
    message_t msg = {0};
    strcpy(msg.type, "DATA");
    strcpy(msg.code, GROUP_CODE);
    sprintf(msg.data, "LED %s", led_on ? "ON" : "OFF");

    for (int i = 0; i < peer_count; i++)
    {
        print_mac("Enviando DATA para", peers[i].peer_addr);
        esp_err_t err = esp_now_send(peers[i].peer_addr, (uint8_t *)&msg, sizeof(msg));
        if (err != ESP_OK)
            ESP_LOGE(TAG, "Falha ao enviar para peer %d: %s", i, esp_err_to_name(err));
    }
}

void wifi_send_ir(const char *id, const uint32_t *pulses, int pulse_len)
{
    if (pulse_len <= 0 || pulse_len > MAX_PULSES)
    {
        ESP_LOGW(TAG, "Quantidade de pulsos IR inválida: %d", pulse_len);
        return;
    }

    ir_message_t ir_msg = {0};
    strncpy(ir_msg.id, id, sizeof(ir_msg.id) - 1);
    ir_msg.pulse_len = pulse_len;
    memcpy(ir_msg.pulse_data, pulses, pulse_len * sizeof(uint32_t));

    for (int i = 0; i < peer_count; i++)
    {
        print_mac("Enviando IR para", peers[i].peer_addr);
        esp_err_t err = esp_now_send(peers[i].peer_addr, (uint8_t *)&ir_msg, sizeof(ir_msg));
        if (err != ESP_OK)
            ESP_LOGE(TAG, "Erro ao enviar IR para peer %d: %s", i, esp_err_to_name(err));
    }

    ESP_LOGI(TAG, "IR enviado para %d peers (ID: %s)", peer_count, ir_msg.id);
}

// ------------------- Funções internas -------------------

static void init_wifi(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_ERROR_CHECK(esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE));
    ESP_ERROR_CHECK(esp_wifi_set_protocol(WIFI_IF_STA,
                                          WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N));
}

static void wait_for_wifi_ready(void)
{
    wifi_second_chan_t second;
    uint8_t channel = 0;

    for (int retry = 0; retry < 20; retry++)
    {
        esp_err_t err = esp_wifi_get_channel(&channel, &second);
        if (err == ESP_OK && channel > 0)
        {
            ESP_LOGI(TAG, "Wi-Fi ativo no canal %d", channel);
            return;
        }
        ESP_LOGW(TAG, "Aguardando Wi-Fi estabilizar...(%d)", retry + 1);
        vTaskDelay(pdMS_TO_TICKS(200));
    }

    ESP_LOGE(TAG, "Timeout wifi");
}

static void init_espnow(void)
{
    uint8_t bcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

    if (esp_now_is_peer_exist(bcast))
        ESP_ERROR_CHECK(esp_now_deinit());

    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_recv_cb(on_data_recv));
    ESP_ERROR_CHECK(esp_now_register_send_cb(on_data_sent));

    esp_now_peer_info_t peer = {
        .channel = 1,
        .ifidx = WIFI_IF_STA,
        .encrypt = false};
    memcpy(peer.peer_addr, bcast, 6);

    if (!esp_now_is_peer_exist(bcast))
    {
        esp_err_t err = esp_now_add_peer(&peer);
        if (err == ESP_OK)
            ESP_LOGI(TAG, "Peer de broadcast adicionado");
        else
            ESP_LOGE(TAG, "Erro ao adicionar peer broadcast: %s", esp_err_to_name(err));
    }

    uint8_t my_mac[6];
    esp_read_mac(my_mac, ESP_MAC_WIFI_STA);
    print_mac("Meu MAC é:", my_mac);
}

static void on_data_recv(const esp_now_recv_info_t *recv_info, const uint8_t *data, int data_len)
{
    const uint8_t *mac = recv_info->src_addr;

    uint8_t my_mac[6];
    esp_read_mac(my_mac, ESP_MAC_WIFI_STA);
    if (memcmp(mac, my_mac, 6) == 0)
    {
        ESP_LOGW(TAG, "Ignorando mensagem do próprio MAC.");
        return;
    }

    if (data_len == sizeof(message_t))
    {
        const message_t *msg = (const message_t *)data;

        print_mac(msg->type, mac);
        ESP_LOGI(TAG, "Conteúdo: '%s'", msg->data);

        if (strcmp(msg->code, GROUP_CODE) != 0)
        {
            ESP_LOGW(TAG, "Código de grupo inválido: %s", msg->code);
            return;
        }

        if (strcmp(msg->type, "HELLO") == 0)
        {
            add_peer_if_needed(mac);
            send_welcome(mac);
        }
        else if (strcmp(msg->type, "WELCOME") == 0)
        {
            add_peer_if_needed(mac);
        }
        else if (strcmp(msg->type, "DATA") == 0)
        {
            add_peer_if_needed(mac);
            // Ex: interpretar conteúdo de msg->data
        }
    }
    else if (data_len == sizeof(ir_message_t))
    {
        const ir_message_t *ir = (const ir_message_t *)data;
        print_mac("IR recebido de", mac);
        ESP_LOGI(TAG, "Destino: %s (%d pulsos)", ir->id, ir->pulse_len);

        // Ação local com base no ID
        if (strcmp(ir->id, "1") == 0 || strcmp(ir->id, "TODOS") == 0)
        {
            // Manda reproduzir localmente
            ir_send_raw(ir->pulse_data, ir->pulse_len);
        }
    }
    else
    {
        ESP_LOGW(TAG, "Mensagem com tamanho inesperado: %d bytes", data_len);
    }
}

static void on_data_sent(const uint8_t *mac_addr, esp_now_send_status_t status)
{
    print_mac("Enviado para", mac_addr);
    ESP_LOGI(TAG, "Status: %s", (status == ESP_NOW_SEND_SUCCESS) ? "SUCESSO" : "FALHA");
}

static void add_peer_if_needed(const uint8_t *mac)
{
    for (int i = 0; i < peer_count; i++)
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
            print_mac("Novo peer:", mac);
        }
        else
        {
            ESP_LOGE(TAG, "Erro ao adicionar peer: %s", esp_err_to_name(err));
        }
    }
    else
    {
        ESP_LOGW(TAG, "Limite máximo de peers atingido!");
    }
}

static void send_welcome(const uint8_t *dest_mac)
{
    message_t msg = {0};
    strcpy(msg.type, "WELCOME");
    strcpy(msg.code, GROUP_CODE);
    strcpy(msg.data, "MAC OK");

    print_mac("Enviando WELCOME para", dest_mac);
    esp_err_t err = esp_now_send(dest_mac, (uint8_t *)&msg, sizeof(msg));
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Falha ao enviar WELCOME: %s", esp_err_to_name(err));
    }
}

static void print_mac(const char *prefix, const uint8_t *mac)
{
    ESP_LOGI(TAG, "%s %02X:%02X:%02X:%02X:%02X:%02X",
             prefix, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}
