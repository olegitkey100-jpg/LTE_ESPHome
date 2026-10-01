#include "ppp_modem_component.h"
#include "esphome/core/log.h"
#include "esp_system.h"
#include "esp_check.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "esp_netif.h"
#include "esp_netif_ppp.h"
#include "usb/usb_host.h"

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

static void client_event_callback(const usb_host_client_event_msg_t *event_msg, void *arg) {
    switch (event_msg->type) {
        case USB_HOST_CLIENT_EVENT_NEW_DEV:
            ESP_LOGI(TAG, "New USB device connected, address: %d", event_msg->new_dev.address);
            break;
        case USB_HOST_CLIENT_EVENT_DEV_GONE:
            ESP_LOGI(TAG, "USB device disconnected, address: %d", event_msg->dev_gone.dev_hdl);
            break;
        default:
            break;
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
    ESP_LOGI(TAG, "USB OTG pins initialized for SIM7670G");
#endif
}

void PppModemComponent::setup() {
    ESP_LOGI(TAG, "Setting up SIM7670G 4G Modem component...");

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

    // Ініціалізація USB Host стека
    const usb_host_config_t host_config = {
        .skip_phy_setup = false,
        .intr_flags = ESP_INTR_FLAG_LEVEL1,
    };
    ret = usb_host_install(&host_config);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Failed to install USB Host stack: %s", esp_err_to_name(ret));
        return;
    }

    // Реєстрація клієнта USB Host для відстеження підключення пристроїв (VID: 0x05C6, PID: 0x9330)
    usb_host_client_config_t client_config = {
        .is_synchronous = false,
        .max_num_event_msg = 5,
        .async = {
            .client_callback = client_event_callback,
            .arg = nullptr,
        },
    };
    usb_host_client_handle_t client_hdl;
    ret = usb_host_client_register(&client_config, &client_hdl);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register USB host client: %s", esp_err_to_name(ret));
        return;
    }

    ESP_LOGI(TAG, "USB Host and Client successfully initialized for SIM7670G. APN: %s", this->apn_.c_str());
}

void PppModemComponent::loop() {
    bool all_events_handled = false;
    usb_host_lib_handle_events(pdMS_TO_TICKS(10), &all_events_handled);

    if (this->event_group_ != nullptr) {
        EventBits_t bits = xEventGroupGetBits(this->event_group_);
        if (bits & EVENT_GOT_IP_BIT) {
            // Модем успішно отримав IP-адресу через PPP
        }
    }
}

void PppModemComponent::dump_config() {
    ESP_LOGCONFIG(TAG, "SIM7670G 4G Modem Component:");
    ESP_LOGCONFIG(TAG, "  APN: %s", this->apn_.c_str());
}

}  // namespace ppp_modem
}  // namespace esphome
