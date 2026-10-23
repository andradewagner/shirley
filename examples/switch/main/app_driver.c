#include <sdkconfig.h>
#include <esp_log.h>
#include <driver/gpio.h>
#include <esp_rmaker_core.h>
#include <esp_rmaker_standard_params.h>
#include <iot_button.h>
#include <button_gpio.h>
#include <app_reset.h>
#include "app_priv.h"

#define BUTTON_GPIO          CONFIG_EXAMPLE_BOARD_BUTTON_GPIO
#define BUTTON_ACTIVE_LEVEL  0

#define WIFI_RESET_BUTTON_TIMEOUT       3
#define FACTORY_RESET_BUTTON_TIMEOUT    10

// Pinos GPIO conectados aos 4 canais do módulo de relé
static const gpio_num_t RELAY_GPIOS[NUM_RELAYS] = {
    GPIO_NUM_4,  // Relé 1 (Válvula 1)
    GPIO_NUM_5,  // Relé 2 (Válvula 2)
    GPIO_NUM_6,  // Relé 3 (Válvula 3)
    GPIO_NUM_7   // Relé 4 (Válvula 4)
};

static bool g_relay_states[NUM_RELAYS] = {DEFAULT_POWER, DEFAULT_POWER, DEFAULT_POWER, DEFAULT_POWER};
static const char *TAG = "app_driver";

static void set_relay_hardware_state(int index, bool target)
{
    if (index >= 0 && index < NUM_RELAYS) {
        gpio_set_level(RELAY_GPIOS[index], target ? 1 : 0);
        ESP_LOGI(TAG, "Relé %d (GPIO %d) -> %s", index + 1, RELAY_GPIOS[index], target ? "LIGADO" : "DESLIGADO");
    }
}

static void push_btn_cb(void *arg, void *data)
{
    bool new_state = !g_relay_states[0];
    app_driver_set_state(0, new_state);
    
    if (relay_devices[0]) {
        esp_rmaker_param_update_and_report(
            esp_rmaker_device_get_param_by_name(relay_devices[0], ESP_RMAKER_DEF_POWER_NAME),
            esp_rmaker_bool(new_state)
        );
    }
}

void app_driver_init(void)
{
    // Inicialização correta do botão usando a API nativa do exemplo original
    button_config_t btn_cfg = {
        .long_press_time = 0,
        .short_press_time = 0,
    };
    button_gpio_config_t gpio_cfg = {
        .gpio_num = BUTTON_GPIO,
        .active_level = BUTTON_ACTIVE_LEVEL,
        .enable_power_save = false,
    };
    button_handle_t btn_handle = NULL;
    if (iot_button_new_gpio_device(&btn_cfg, &gpio_cfg, &btn_handle) == ESP_OK && btn_handle) {
        iot_button_register_cb(btn_handle, BUTTON_SINGLE_CLICK, NULL, push_btn_cb, NULL);
        app_reset_button_register(btn_handle, WIFI_RESET_BUTTON_TIMEOUT, FACTORY_RESET_BUTTON_TIMEOUT);
    }

    // Configuração dos pinos dos 4 Relés como Saída
    uint64_t pin_mask = 0;
    for (int i = 0; i < NUM_RELAYS; i++) {
        pin_mask |= ((uint64_t)1 << RELAY_GPIOS[i]);
    }

    gpio_config_t io_conf = {
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = 0,
        .pull_down_en = 1,
        .pin_bit_mask = pin_mask,
    };
    gpio_config(&io_conf);

    for (int i = 0; i < NUM_RELAYS; i++) {
        set_relay_hardware_state(i, DEFAULT_POWER);
    }
}

int IRAM_ATTR app_driver_set_state(int relay_index, bool state)
{
    if (relay_index >= 0 && relay_index < NUM_RELAYS) {
        if (g_relay_states[relay_index] != state) {
            g_relay_states[relay_index] = state;
            set_relay_hardware_state(relay_index, state);
        }
        return ESP_OK;
    }
    return ESP_ERR_INVALID_ARG;
}

bool app_driver_get_state(int relay_index)
{
    if (relay_index >= 0 && relay_index < NUM_RELAYS) {
        return g_relay_states[relay_index];
    }
    return false;
}