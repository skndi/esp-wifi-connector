#pragma once

#include <functional>
#include <memory>

#include "esp_netif_types.h"
#include "esp_wifi.h"
#include "freertos/event_groups.h"

struct EventGroupDef_t;
typedef struct EventGroupDef_t* EventGroupHandle_t;

typedef int esp_err_t;

extern "C" void eventHandlerWrapper(void* arg, esp_event_base_t eventBase, int32_t eventId, void* eventData);

namespace wifi {

class WifiConnector {
public:
    WifiConnector();
    esp_err_t connect(const std::string& ssid, const std::string& password);

private:
    friend void ::eventHandlerWrapper(void* arg, esp_event_base_t eventBase, int32_t eventId, void* eventData);
    void eventHandler(esp_event_base_t eventBase, int32_t eventId, void* eventData);

    using EventGroupDeleter = std::function<void(EventGroupDef_t*)>;
    std::unique_ptr<EventGroupDef_t, EventGroupDeleter> m_wifiEventGroup;

    using NetifHandleDeleter = std::function<void(esp_netif_t*)>;
    std::unique_ptr<esp_netif_t, NetifHandleDeleter> m_netifHandle;

    int32_t m_retryNum{};

    static constexpr int32_t WIFI_CONNECTED_BIT = BIT0;
    static constexpr int32_t WIFI_FAIL_BIT = BIT1;
};
}  // namespace wifi