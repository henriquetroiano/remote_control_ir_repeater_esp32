#ifndef WIFI_H
#define WIFI_H

#include <esp_err.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  /**
   * @brief Inicializa Wi-Fi (modo STA), aguarda estabilizar
   *        e inicia o ESP-NOW com os callbacks.
   */
  void wifi_init_all(void);

  /**
   * @brief Envia um broadcast "HELLO" para procurar peers.
   */
  void wifi_send_hello(void);

  /**
   * @brief Envia mensagem "DATA" para todos os peers,
   *        indicando (por exemplo) o estado do LED.
   */
  void wifi_send_data_to_peers(bool led_on);

  /**
   * @brief Envia sinal IR para todos os peers, junto com um ID (ex: "TV1", "TODOS", etc.).
   *
   * @param id           Nome/ID do alvo (máx 31 chars)
   * @param pulses       Vetor de pulsos (em microssegundos)
   * @param pulse_len    Quantidade de pulsos no vetor
   */
  void wifi_send_ir(const char *id, const uint32_t *pulses, int pulse_len);

#ifdef __cplusplus
}
#endif

#endif // WIFI_H
