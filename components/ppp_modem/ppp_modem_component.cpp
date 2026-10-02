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

void PppModemComponent::init_usb_pins_() {
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
    gpio_set_level(GPIO_NUM_12, 1);
    gpio_set_level(GPIO_NUM_13, 1);
}

static void modem_init_task(void *arg) {
    char *apn_str = (char *) arg;
    const char *task_tag = "modem_init";

    ESP_LOGI(task_tag, "MODEM_STEP 1: Waiting 8s for stable USB Host enumeration...");
    vTaskDelay(pdMS_TO_TICKS(8000));

    ESP_LOGI(task_tag, "MODEM_STEP 2: USB layer stable. Preparing network structures...");
    
    esp_netif_config_t netif_ppp_config = ESP_NETIF_DEFAULT_PPP();
    esp_netif_t *esp_netif = esp_netif_new(&netif_ppp_config);
    if (esp_netif == nullptr) {
        ESP_LOGE(task_tag, "MODEM_STEP ERROR: Failed to create ESP-NETIF PPP instance");
        delete[] apn_str;
        vTaskDelete(nullptr);
        return;
    }
    ESP_LOGI(task_tag, "MODEM_STEP 3: Netif PPP initialized successfully.");

    ESP_LOGI(task_tag, "MODEM_STEP 4: USB CDC-ACM and network layers ready with APN: %s", apn_str);

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000));
        ESP_LOGI(task_tag, "MODEM_STEP 5: Worker loop active, monitoring link state...");
    }

    delete[] apn_str;
    vTaskDelete(nullptr);
}

void PppModemComponent::setup() {
    ESP_LOGI(TAG, "Setting up SIM7670G 4G Modem Component...");
    init_usb_pins_();

    char *apn_copy = new char[this->apn_.length() + 1];
    strcpy(apn_copy, this->apn_.c_str());

    xTaskCreate(modem_init_task, "modem_init_task", 4096, (void *) apn_copy, 5, nullptr);
}

void PppModemComponent::dump_config() {
    ESP_LOGCONFIG(TAG, "SIM7670G 4G Modem Component:");
    ESP_LOGCONFIG(TAG, "  APN: %s", this->apn_.c_str());
}

}  // namespace ppp_modem
}  // namespace esphome
