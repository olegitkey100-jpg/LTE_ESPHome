#include "ppp_modem_component.h"
#include "esphome/core/log.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "esp_netif.h"
#include "esp_netif_ppp.h"
#include "esp_modem_api.h"

namespace esphome {
namespace ppp_modem {

static const char *TAG = "ppp_modem.component";

PppModemComponent::PppModemComponent() = default;

void PppModemComponent::init_usb_pins_() {
#ifdef CONFIG_ESP32_S3_USB_OTG
    const gpio_config_t io_config = {
        .pin_bit_mask = BIT64(GPIO_NUM_18),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_config);
    gpio_set_level(GPIO_NUM_18, 1);

    const gpio_config_t power_io_config = {
        .pin_bit_mask = BIT64(GPIO_NUM_17) | BIT64(GPIO_NUM_12) | BIT64(GPIO_NUM_13),
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
    ESP_LOGI(TAG, "USB OTG pins initialized for SIM7670G");
#endif
}

void PppModemComponent::setup() {
    ESP_LOGI(TAG, "Setting up SIM7670G 4G Modem via esp_modem...");

    this->init_usb_pins_();

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    esp_netif_init();

    // 1. Створення мережевого інтерфейсу PPP
    esp_netif_config_t netif_ppp_config = ESP_NETIF_DEFAULT_PPP();
    esp_netif_t *esp_netif = esp_netif_new(&netif_ppp_config);
    if (esp_netif == nullptr) {
        ESP_LOGE(TAG, "Failed to create ESP-NETIF PPP instance");
        this->mark_failed();
        return;
    }

    // 2. Конфігурація DTE та DCE
    esp_modem_dte_config_t dte_config = ESP_MODEM_DTE_DEFAULT_CONFIG();
    esp_modem_dce_config_t dce_config = ESP_MODEM_DCE_DEFAULT_CONFIG(this->apn_.c_str());

    // Створення USB DTE об'єкта для керування модемом через USB Host
    esp_modem_dte_t *dte = esp_modem_dte_new_usb(&dte_config);
    if (dte == nullptr) {
        ESP_LOGE(TAG, "Failed to create USB DTE for modem");
        this->mark_failed();
        return;
    }

    // Створення DCE об'єкта для SIM7670
    esp_modem_dce_t *dce = esp_modem_dce_new_sim7600(&dte, &dce_config);
    if (dce == nullptr) {
        ESP_LOGE(TAG, "Failed to create SIM7600/7670 DCE object");
        this->mark_failed();
        return;
    }

    // Зв'язування з мережевим інтерфейсом та запуск PPP
    esp_modem_set_default_netif(dce, esp_netif);

    ESP_LOGI(TAG, "USB Modem DTE & DCE successfully initialized. Target APN: %s", this->apn_.c_str());
}
void PppModemComponent::loop() {
}

void PppModemComponent::dump_config() {
    ESP_LOGCONFIG(TAG, "SIM7670G 4G Modem Component (esp_modem):");
    ESP_LOGCONFIG(TAG, "  APN: %s", this->apn_.c_str());
}

}  // namespace ppp_modem
}  // namespace esphome
