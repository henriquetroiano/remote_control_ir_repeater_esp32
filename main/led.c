#include "led.h"
#include "driver/gpio.h"
#include "sdkconfig.h"  // Para acessar CONFIG_BLINK_GPIO, se definido no menuconfig

// GPIO definido no menuconfig: CONFIG_BLINK_GPIO
// Se preferir, pode fazer #define BLINK_GPIO 2 ou outro pino direto aqui.

static bool s_led_state = false; // estado interno do LED

void led_init(void)
{
    gpio_reset_pin(CONFIG_BLINK_GPIO);
    gpio_set_direction(CONFIG_BLINK_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level(CONFIG_BLINK_GPIO, 0);
    s_led_state = false;
}

void led_toggle(void)
{
    s_led_state = !s_led_state;
    gpio_set_level(CONFIG_BLINK_GPIO, s_led_state);
}

bool led_get_state(void)
{
    return s_led_state;
}
