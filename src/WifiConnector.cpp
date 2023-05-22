#include "WifiConnector.hpp"

#include <esp_err.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <string.h>

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(array) (sizeof(array) / sizeof((array)[0]))
#endif

#define MAXIMUM_RETRY 5
static const char* TAG = "wifi station";

using namespace wifi;

WifiConnector::WifiConnector() {}

void WifiConnector::eventHandler(esp_event_base_t eventBase, int32_t eventId, void* eventData) {
    if (eventBase == WIFI_EVENT && eventId == WIFI_EVENT_STA_START) {
        ESP_ERROR_CHECK(esp_wifi_connect());
    } else if (eventBase == WIFI_EVENT && eventId == WIFI_EVENT_STA_DISCONNECTED) {
        if (m_retryNum < MAXIMUM_RETRY) {
            ESP_ERROR_CHECK(esp_wifi_connect());
            m_retryNum++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        } else {
            xEventGroupSetBits(m_wifiEventGroup.get(), WIFI_FAIL_BIT);
            m_retryNum = 0;
        }
        ESP_LOGI(TAG, "connect to the AP fail");
    } else if (eventBase == IP_EVENT && eventId == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*)eventData;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        m_retryNum = 0;
        xEventGroupSetBits(m_wifiEventGroup.get(), WIFI_CONNECTED_BIT);
    }
}

void eventHandlerWrapper(void* arg, esp_event_base_t eventBase, int32_t eventId, void* eventData) {
    ESP_LOGI(TAG, "Received event");
    WifiConnector* connector = static_cast<WifiConnector*>(arg);
    connector->eventHandler(eventBase, eventId, eventData);
}

esp_err_t WifiConnector::connect(const std::string& ssid, const std::string& password) {
    m_wifiEventGroup = std::unique_ptr<EventGroupDef_t, EventGroupDeleter>(
        xEventGroupCreate(), [](EventGroupHandle_t eventGroupHandle) { vEventGroupDelete(eventGroupHandle); });
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    m_netifHandle = std::unique_ptr<esp_netif_t, NetifHandleDeleter>(
        esp_netif_create_default_wifi_sta(),
        [](esp_netif_t* netifHandle) { esp_netif_destroy_default_wifi(netifHandle); });

    wifi_init_config_t wifi_config = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wifi_config));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &eventHandlerWrapper, this, &instance_any_id);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &eventHandlerWrapper, this, &instance_got_ip);

    wifi_config_t wifiConfig{};
    wifiConfig.sta.ssid[0] = '\0';
    strncat((char*)wifiConfig.sta.ssid, ssid.c_str(), ARRAY_SIZE(wifiConfig.sta.ssid) - 1);

    wifiConfig.sta.password[0] = '\0';
    strncat((char*)wifiConfig.sta.password, password.c_str(), ARRAY_SIZE(wifiConfig.sta.password) - 1);

    ESP_LOGI(TAG, "ssid: %s \tpassword: %s\n", wifiConfig.sta.ssid, wifiConfig.sta.password);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifiConfig));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi_init_sta finished.");

    EventBits_t bits = xEventGroupWaitBits(
        m_wifiEventGroup.get(), WIFI_CONNECTED_BIT | WIFI_FAIL_BIT, pdFALSE, pdFALSE, portMAX_DELAY);

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "connected to ap SSID:%s password:%s", ssid.c_str(), password.c_str());
        return ESP_OK;
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGE(TAG, "Failed to connect to SSID:%s, password:%s", ssid.c_str(), password.c_str());
        return ESP_FAIL;
    } else {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
        return ESP_FAIL;
    }
}