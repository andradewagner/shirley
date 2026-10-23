#include <string.h>
#include <inttypes.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <esp_event.h>
#include <nvs_flash.h>

#include <esp_rmaker_core.h>
#include <esp_rmaker_standard_types.h>
#include <esp_rmaker_standard_params.h>
#include <esp_rmaker_standard_devices.h>
#include <esp_rmaker_schedule.h>
#include <esp_rmaker_ota.h>

#include <app_network.h>
#include <app_insights.h>

#include "app_priv.h"

static const char *TAG = "app_main";
esp_rmaker_device_t *relay_devices[NUM_RELAYS];

/* Callback executado quando o cliente comanda qualquer um dos relés pelo Telemóvel */
static esp_err_t write_cb(const esp_rmaker_device_t *device, const esp_rmaker_param_t *param,
            const esp_rmaker_param_val_t val, void *priv_data, esp_rmaker_write_ctx_t *ctx)
{
    if (strcmp(esp_rmaker_param_get_name(param), ESP_RMAKER_DEF_POWER_NAME) == 0) {
        int relay_index = (int)(intptr_t)priv_data;
        
        ESP_LOGI(TAG, "Comando nuvem -> Relé %d: %s", relay_index + 1, val.val.b ? "LIGADO" : "DESLIGADO");
        app_driver_set_state(relay_index, val.val.b);
        esp_rmaker_param_update(param, val);
    }
    return ESP_OK;
}

static void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    if (event_base == APP_NETWORK_EVENT && event_id == APP_NETWORK_EVENT_QR_DISPLAY) {
        ESP_LOGI(TAG, "QR Code de Provisionamento: %s", (char *)event_data);
    }
}

void app_main()
{
    app_driver_init();

    // 1. Inicializa o NVS IMEDIATAMENTE antes de tudo
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS com problemas ou nova versão encontrada. Apagando e reiniciando NVS...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);    

    // 1. Inicializa o subsistema de rede e registra o handler
    app_network_init();
    ESP_ERROR_CHECK(esp_event_handler_register(APP_NETWORK_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));

    // 3. ESSENCIAL: Ative a sincronização de tempo (true) para validação dos certificados SSL/TLS da AWS
    esp_rmaker_config_t rainmaker_cfg = {
        .enable_time_sync = true, 
    };
    
    esp_rmaker_node_t *node = esp_rmaker_node_init(&rainmaker_cfg, "Projeto Shirley Garden", "Multi-Relay Controller");
    if (!node) {
        ESP_LOGE(TAG, "Falha ao inicializar o nó RainMaker.");
        vTaskDelay(5000 / portTICK_PERIOD_MS);
        abort();
    }

    // Criar dinamicamente os 4 dispositivos de relé no RainMaker
    char device_name[32];
    for (int i = 0; i < NUM_RELAYS; i++) {
        snprintf(device_name, sizeof(device_name), "Valvula 12V - Zona %d", i + 1);
        
        relay_devices[i] = esp_rmaker_device_create(device_name, ESP_RMAKER_DEVICE_SWITCH, (void *)(intptr_t)i);
        esp_rmaker_device_add_cb(relay_devices[i], write_cb, (void *)(intptr_t)i);
        
        esp_rmaker_device_add_param(relay_devices[i], esp_rmaker_name_param_create(ESP_RMAKER_DEF_NAME_PARAM, device_name));
        
        esp_rmaker_param_t *power_param = esp_rmaker_power_param_create(ESP_RMAKER_DEF_POWER_NAME, DEFAULT_POWER);
        esp_rmaker_device_add_param(relay_devices[i], power_param);
        esp_rmaker_device_assign_primary_param(relay_devices[i], power_param);
        
        esp_rmaker_node_add_device(node, relay_devices[i]);
    }

    esp_rmaker_ota_enable_default();

    // Habilitar serviço de fuso horário
    ESP_ERROR_CHECK(esp_rmaker_timezone_service_enable());

    // Habilitar agendamentos
    ESP_ERROR_CHECK(esp_rmaker_schedule_enable());
    
    // 4. Inicie o RainMaker APENAS após estar conectado e com relógio minimamente sincronizado
    esp_rmaker_start();

    // 2. IMPORTANTE: Conecta na rede PRIMEIRO para obter IP e estabelecer rotas
    err = app_network_start(POP_TYPE_MAC);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao iniciar a rede. A abortar...");
        vTaskDelay(5000 / portTICK_PERIOD_MS);
        abort();
    }
}