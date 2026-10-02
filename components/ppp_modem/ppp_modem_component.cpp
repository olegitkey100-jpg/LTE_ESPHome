#include "ppp_modem_component.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "driver/gpio.h"
#include "usb/usb_host.h"
#include "esp_netif.h"
#include "esp_netif_ppp.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace esphome {
namespace ppp_modem {

static const char *TAG = "ppp_modem.component";

PppModemComponent::PppModemComponent() = default;

static void client_event_callback(const usb_host_client_event_msg_t *event_msg, void *arg) {
    const char *client_tag = "usb_client";
    switch (event_msg->event) {
        case USB_HOST_CLIENT_EVENT_NEW_DEV:
            ESP_LOGI(client_tag, ">>> New USB device detected on bus! Address: %d", event_msg->new_dev.address);
            break;
        case USB_HOST_CLIENT_EVENT_DEV_GONE:
            ESP_LOGW(client_tag, "<<< USB device disconnected");
            break;
        default:
            ESP_LOGD(client_tag, "USB client event: %d", event_msg->event);
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
    esp_err_t reg_err = usb_host_client_register(&client_config, &client_handle);
    if (reg_err != ESP_OK) {
        ESP_LOGE(task_tag, "Failed to register USB host client: %s", esp_err_to_name(reg_err));
        vTaskDelete(nullptr);
        return;
    }
    ESP_LOGI(task_tag, "USB host client registered successfully.");

    while (1) {
        uint32_t event_flags;
        esp_err_t err = usb_host_lib_handle_events(pdMS_TO_TICKS(1000), &event_flags);
        if (err == ESP_OK) {
            usb_host_client_handle_events(client_handle, pdMS_TO_TICKS(50));
        }
    }
}

static void modem_delayed_init_task(void *arg) {
    char *apn_str = (char *) arg;
    const char *task_tag = "modem_init";
    
    ESP_LOGI(task_tag, "MODEM_INIT: Applying proper power-on sequence for SIM7670G...");
    
    // Апаратне управління живленням модема для гарантованого холодного старту
    gpio_set_level(GPIO_NUM_18, 1);
    gpio_set_level(GPIO_NUM_17, 0); // Вимикаємо живлення
    gpio_set_level(GPIO_NUM_12, 0);
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    gpio_set_level(GPIO_NUM_17, 1); // Вмикаємо живлення
    vTaskDelay(pdMS_TO_TICKS(500));
    gpio_set_level(GPIO_NUM_12, 1); // PWRKEY імпульс
    vTaskDelay(pdMS_TO_TICKS(2000));
    gpio_set_level(GPIO_NUM_12, 0);

    ESP_LOGI(task_tag, "MODEM_INIT: Waiting 4s for modem internal boot & USB enumeration...");
    vTaskDelay(pdMS_TO_TICKS(4000));

    ESP_LOGI(task_tag, "MODEM_INIT: Installing USB Host library...");
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
    ESP_LOGI(task_tag, "MODEM_INIT: USB Host installed successfully.");

    xTaskCreate(usb_lib_task, "usb_host", 4096, nullptr, 3, nullptr);

    ESP_LOGI(task_tag, "MODEM_INIT: Initializing Netif PPP layer...");
    esp_netif_config_t netif_ppp_config = ESP_NETIF_DEFAULT_PPP();
    esp_netif_t *esp_netif = esp_netif_new(&netif_ppp_config);
    if (esp_netif == nullptr) {
        ESP_LOGE(task_tag, "MODEM_INIT ERROR: Failed to create ESP-NETIF PPP instance");
        delete[] apn_str;
        vTaskDelete(nullptr);
        return;
    }
    ESP_LOGI(task_tag, "MODEM_INIT: Netif PPP ready for APN: %s", apn_str);

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000));
        ESP_LOGI(task_tag, "MODEM_INIT: Monitoring bus...");
    }

    delete[] apn_str;
    vTaskDelete(nullptr);
}

void PppModemComponent::setup() {
    ESP_LOGI(TAG, "PPP Modem Component setup called safely.");
    
    const gpio_config_t power_io_config = {
        .pin_bit_mask = (1ULL << GPIO_NUM_17) | (1ULL << GPIO_NUM_12) | (1ULL << GPIO_NUM_13) | (1ULL << GPIO_NUM_18),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&power_io_config);

    char *apn_copy = new char[this->apn_.length() + 1];
    strcpy(apn_copy, this->apn_.c_str());
    xTaskCreate(modem_delayed_init_task, "modem_init", 4096, apn_copy, 4, nullptr);
}

void PppModemComponent::loop() {
}

void PppModemComponent::dump_config() {
    ESP_LOGCONFIG(TAG, "SIM7670G 4G Modem Component:");
    ESP_LOGCONFIG(TAG, "  APN: %s", this->apn_.c_str());
}

}  // namespace ppp_modem
}  // namespace esphome
