#include "ubt_network.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_websocket_client.h"
#include "esp_event.h"
#include "common.h"
#include "ws_commands.h"

#define TAG "NWM"

esp_websocket_client_handle_t ws_client = NULL;

static void _websocket_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_websocket_event_data_t *data = (esp_websocket_event_data_t *)event_data;
    switch (event_id)
    {
    case WEBSOCKET_EVENT_CONNECTED:
        ESP_LOGI(TAG, "WEBSOCKET_EVENT_CONNECTED");
        xTaskNotify(xAppTask, 1 << NOTIFICATION_NETWORK_UP, eSetBits);
        break;
    case WEBSOCKET_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "WEBSOCKET_EVENT_DISCONNECTED");
        xTaskNotify(xAppTask, 1 << NOTIFICATION_NETWORK_DOWN, eSetBits);
        break;
    case WEBSOCKET_EVENT_DATA:
        switch (data->op_code)
        {
        case 0x9:
            ESP_LOGI(TAG, "RX: ping");
            break;
        case 0xA:
            ESP_LOGI(TAG, "RX: pong");
            break;
        case 0x1:
            ESP_LOGI(TAG, "RX: data (%.*s)", data->data_len, (char *)data->data_ptr);
            ws_commands_process(data->data_ptr, data->data_len);
            break;
        default:
            ESP_LOGW(TAG, "Unprocessed websocket opcode %d", data->op_code);
            break;
        }
        break;
    case WEBSOCKET_EVENT_ERROR:
        ESP_LOGI(TAG, "WEBSOCKET_EVENT_ERROR");
        break;
    }
}

static void _configure_websocket_client(void)
{
    char uri[64] = {0};
    snprintf(uri, 64, "ws://%s:%d/ws", CONFIG_UBT_WEBSOCKET_SERVER_ADDRESS, CONFIG_UBT_WEBSOCKET_SERVER_PORT);
    const esp_websocket_client_config_t ws_cfg = {
        .uri = uri,
        .reconnect_timeout_ms = 2000,
        .network_timeout_ms = 5000};
    ESP_LOGI(TAG, "Connecting to %s", uri);
    ws_client = esp_websocket_client_init(&ws_cfg);
    if (!ws_client) {
	    ESP_LOGE(TAG, "Can not initialize websocket client");
	    return;
    }
    esp_websocket_register_events(ws_client, WEBSOCKET_EVENT_ANY, _websocket_event_handler, NULL);
    esp_websocket_client_start(ws_client);
}

static void _wifi_event_handler(void *arg, esp_event_base_t event_base,
                                int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && (event_id == WIFI_EVENT_STA_START || event_id == WIFI_EVENT_STA_DISCONNECTED))
    {
        esp_wifi_connect();
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "Connected to gateway with IP " IPSTR, IP2STR(&event->ip_info.ip));
        _configure_websocket_client();
    }
}

static void _configure_nvs()
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
}

void ubt_network_start(void)
{
    esp_log_level_set(TAG, ESP_LOG_DEBUG);

    _configure_nvs();

    ESP_ERROR_CHECK(esp_netif_init());
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &_wifi_event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &_wifi_event_handler,
                                                        NULL,
                                                        &instance_got_ip));
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = CONFIG_UBT_AP_SSID,
            .password = CONFIG_UBT_AP_PASSWORD},
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}

void ubt_network_sendmessage(char *pcMessage)
{
    esp_websocket_client_send_text(ws_client, pcMessage, strlen(pcMessage), portMAX_DELAY);
}
