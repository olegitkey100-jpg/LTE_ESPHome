#include "ppp_modem_component.h"
#include "esphome/core/log.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "esp_netif.h"
#include "esp_netif_ppp.h"
#include "usb/usb_host.h"
#include "esp_modem_api.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace esphome {
namespace ppp_modem {

static const char *TAG = "ppp_modem.component";

PppModemComponent::PppModemComponent() = default;

static void init_usb_pins_() {
    const gpio_config_t io_config = {
        .pin_bit_mask = 1ULL << GPIO_NUM_18,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_config);
    gpio_set_level(GPIO_NUM_18, 1);

    const gpio_config_t power_io_config = {
        .pin_bit_mask = (1ULL << GPIO_NUM_17) | (1ULL << GPIO_NUM_12) | (1ULL << GPIO_NUM_13),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&power_io_config);

    gpio_set_level(GPIO_NUM_17, 1);
    gpio_set_level(GPIO_NUM_12, 0);
    gpio_set_level(GPIO_NUM_13, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(GPIO_NUM_12, 1);
}

static void client_event_callback(const usb_host_client_event_msg_t *event_msg, void *arg) {
    const char *client_tag = "usb_client";
    switch (event_msg->event) {
        case USB_HOST_CLIENT_EVENT_NEW_DEV:
            ESP_LOGI(client_tag, "New USB device detected on the bus (Address: %d)", event_msg->new_dev.address);
            break;
        case USB_HOST_CLIENT_EVENT_DEV_GONE:
            ESP_LOGW(client_tag, "USB device disconnected");
            break;
        default:
            break;
    }
}

static void usb_lib_task(void *arg) {
    const char *task_tag = "usb_host_task";
    
    usb_host_client_config_t client_config;
    memset(&client_config, 0, sizeof(client_config));
    client_config.max_num_event_msg = 5;
    client_config.async.client_event_callback = client_event_callback;
    client_config.async.callback_arg = nullptr;

    usb_host_client_handle_t client_handle;
    if (usb_host_client_register(&client_config, &client_handle) != ESP_OK) {
        ESP_LOGE(task_tag, "Failed to register USB host client");
        vTaskDelete(nullptr);
        return;
    }

    while (1) {
        uint32_t event_flags;
        esp_err_t err = usb_host_lib_handle_events(pdMS_TO_TICKS(2000), &event_flags);
        if (err == ESP_OK) {
            usb_host_client_handle_events(client_handle, pdMS_TO_TICKS(50));
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

static void modem_init_task(void *arg) {
    char *apn_str = (char *) arg;
    const char *task_tag = "modem_init";

    // Даємо повні 5 секунд на старт Wi-Fi, MQTT та API серверів ESPHome
    ESP_LOGI(task_tag, "MODEM_STEP 0: Waiting 5s for ESPHome API and Network to start up...");
    vTaskDelay(pdMS_TO_TICKS(5000));

    ESP_LOGI(task_tag, "MODEM_STEP 1: Initializing USB power pins...");
    init_usb_pins_();

    ESP_LOGI(task_tag, "MODEM_STEP 2: Installing USB Host library...");
    const usb_host_config_t host_config = {
        .skip_phy_setup = false,
        .intr_flags = ESP_INTR_FLAG_LEVEL1,
    };
    esp_err_t ret = usb_host_install(&host_config);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(task_tag, "Failed to install USB Host library: %s", esp_err_to_name(ret));
        delete[] apn_str;
        vTaskDelete(nullptr);
        return;
    }

    xTaskCreate(usb_lib_task, "usb_host", 4096, nullptr, 3, nullptr);
    ESP_LOGI(task_tag, "MODEM_STEP 3: USB host task spawned, waiting 3s for bus stability...");
    vTaskDelay(pdMS_TO_TICKS(3000));

    ESP_LOGI(task_tag, "MODEM_STEP 4: Preparing Netif PPP structures for APN: %s", apn_str);
    esp_netif_config_t netif_ppp_config = ESP_NETIF_DEFAULT_PPP();
    esp_netif_t *esp_netif = esp_netif_new(&netif_ppp_config);
    if (esp_netif == nullptr) {
        ESP_LOGE(task_tag, "MODEM_STEP ERROR: Failed to create ESP-NETIF PPP instance");
        delete[] apn_str;
        vTaskDelete(nullptr);
        return;
    }
    ESP_LOGI(task_tag, "MODEM_STEP 5: Netif PPP initialized successfully.");

    while (1) {
        ESP_LOGI(task_tag, "MODEM_STEP 6: Worker alive, API should be fully working.");
        vTaskDelay(pdMS_TO_TICKS(10000));
    }

    delete[] apn_str;
    vTaskDelete(nullptr);
}

void PppModemComponent::setup() {
    ESP_LOGI(TAG, "=== setup() starting non-blocking init ===");

    // NVS ініціалізація
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    // МИТТЄВО виходимо з setup(), щоб запустився API, а всю важку роботу кидаємо у фоновий таск
    char *apn_copy = new char[this->apn_.length() + 1];
    strcpy(apn_copy, this->apn_.c_str());
    xTaskCreate(modem_init_task, "modem_init", 4096, apn_copy, 4, nullptr);

    ESP_LOGI(TAG, "=== setup() finished instantly, API unblocked ===");
}

void PppModemComponent::loop() {
}

void PppModemComponent::dump_config() {
    ESP_LOGCONFIG(TAG, "SIM7670G 4G Modem Component:");
    ESP_LOGCONFIG(TAG, "  APN: %s", this->apn_.c_str());
}

}  // namespace ppp_modem
}  // namespace esphome
