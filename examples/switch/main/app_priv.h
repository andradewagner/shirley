#pragma once
#include <stdint.h>
#include <stdbool.h>

#define NUM_RELAYS 4
#define DEFAULT_POWER false

// Array para armazenar os handles dos dispositivos no RainMaker
extern esp_rmaker_device_t *relay_devices[NUM_RELAYS];

void app_driver_init(void);
int app_driver_set_state(int relay_index, bool state);
bool app_driver_get_state(int relay_index);