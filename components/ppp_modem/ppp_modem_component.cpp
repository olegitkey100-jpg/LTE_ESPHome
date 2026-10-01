#include "ppp_modem_component.h"
#include "esphome/core/log.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "iot_usbh_modem.h"

namespace esphome {
namespace ppp_modem {

static const char *TAG = "ppp_modem.component";

// Список підтримуваних ідентифікаторів нашого модема SIM7670G
static const usb_modem_id_t s_modem_id_list[] = {
    { .vendor_id = 0x05C6, .product_id = 0x9330 },
    { 0 } // Термінатор списку
};

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
    ESP_LOGI(TAG, "Setting up SIM7670G 4G Modem via iot_usbh_modem...");

    this->init_usb_pins_();

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    // Конфігурація підсистеми USB модема
    usbh_modem_config_t modem_config = {
        .modem_id_list = s_modem_id_list,
        .at_tx_buffer_size = 512,
        .at_rx_buffer_size = 512,
        .pdp = {
            .enable = true,
            .cid = 1,
            .type = "IP",
            .apn = this->apn_.c_str()
        }
    };

    // Встановлення драйвера через iot_usbh_modem
    ret = usbh_modem_install(&modem_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to install USB modem subsystem: %s", esp_err_to_name(ret));
        this->mark_failed();
        return;
    }

    // Увімкнення автоматичного встановлення PPP-з'єднання при появі пристрою
    usbh_modem_ppp_auto_connect(true);

    ESP_LOGI(TAG, "USB modem subsystem successfully installed. Target APN: %s", this->apn_.c_str());
}

void PppModemComponent::loop() {
    // Внутрішні задачі та обробка подій модема керуються фоновими тасками iot_usbh_modem
}

void PppModemComponent::dump_config() {
    ESP_LOGCONFIG(TAG, "SIM7670G 4G Modem Component (iot_usbh_modem):");
    ESP_LOGCONFIG(TAG, "  APN: %s", this->apn_.c_str());
}

}  // namespace ppp_modem
}  // namespace esphome
