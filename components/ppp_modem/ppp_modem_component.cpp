#include "ppp_modem_component.h"
#include "esphome/core/log.h"
#include "esp_system.h"
#include "esp_check.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "iot_usbh_cdc.h"
#include "esp_netif.h"
#include "esp_netif_ppp.h"
#include "esp_ip4_addr.h"


namespace esphome {
namespace ppp_modem {

static const char *TAG = "ppp_modem.component";

#define EVENT_GOT_IP_BIT (BIT0)

static void ppp_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    EventGroupHandle_t event_group = static_cast<EventGroupHandle_t>(arg);
    if (event_base == IP_EVENT) {
        if (event_id == IP_EVENT_PPP_GOT_IP) {
            ESP_LOGI(TAG, "PPP network got IP address");
            xEventGroupSetBits(event_group, EVENT_GOT_IP_BIT);
        } else if (event_id == IP_EVENT_PPP_LOST_IP) {
            ESP_LOGW(TAG, "PPP network lost IP address");
            xEventGroupClearBits(event_group, EVENT_GOT_IP_BIT);
        }
    }
}

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
    ESP_LOGI(TAG, "USB OTG pins initialized for ESP32-S3");
#endif
}

void PppModemComponent::setup() {
    ESP_LOGI(TAG, "Setting up PPP 4G Modem component...");

    esp_log_level_set("esp-modem", ESP_LOG_WARN);
    esp_log_level_set("esp-modem-dte", ESP_LOG_WARN);

    this->init_usb_pins_();

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    esp_netif_init();

    this->event_group_ = xEventGroupCreate();
    if (this->event_group_ == nullptr) {
        ESP_LOGE(TAG, "Failed to create event group");
        this->mark_failed();
        return;
    }

    esp_event_handler_register(IP_EVENT, IP_EVENT_PPP_GOT_IP, ppp_event_handler, this->event_group_);
    esp_event_handler_register(IP_EVENT, IP_EVENT_PPP_LOST_IP, ppp_event_handler, this->event_group_);

    usbh_cdc_driver_config_t cdc_config = {
        .task_stack_size = 1024 * 4,
        .task_priority = configMAX_PRIORITIES - 1,
        .task_coreid = 0,
        .skip_init_usb_host_driver = false,
    };
    usbh_cdc_driver_install(&cdc_config);

    ESP_LOGI(TAG, "USB CDC driver installed successfully");
}

void PppModemComponent::loop() {
    if (this->event_group_ != nullptr) {
        EventBits_t bits = xEventGroupGetBits(this->event_group_);
        if (bits & EVENT_GOT_IP_BIT) {
            // Модем має IP
        }
    }
}

void PppModemComponent::dump_config() {
    ESP_LOGCONFIG(TAG, "PPP 4G Modem Component:");
    ESP_LOGCONFIG(TAG, "  APN: %s", this->apn_.c_str());
}

}  // namespace ppp_modem
}  // namespace esphome
