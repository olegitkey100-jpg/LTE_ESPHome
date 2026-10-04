#include "ppp_modem_component.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "driver/gpio.h"
#include "esp_netif.h"
#include "esp_netif_ppp.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "iot_usbh_modem.h"
#include "iot_usbh_cdc.h"

namespace esphome {
namespace ppp_modem {

static const char *TAG = "ppp_modem.component";

// Список сумісних 4G модемів (включно з SIM7670G)
static const usb_modem_id_t usb_modem_id_list[] = {
    {
        .match_id = {USB_DEVICE_ID_MATCH_VID_PID, 0x1E0E, 0x9011},
        .interface_idx = 2,
        .ep_addr = -1,
        .description = "SIMCOM, A7600C1/SIMCOM, A7670E"
    },
    {
        .match_id = {USB_DEVICE_ID_MATCH_VID_PID, 0x05C6, 0x9330},
        .interface_idx = 2,
        .ep_addr = -1,
        .description = "SIMCOM, SIM7670G-4G"
    },
    {
        .match_id = {USB_DEVICE_ID_MATCH_VID_PID, 0x2C7C, 0x6001},
        .interface_idx = 4,
        .ep_addr = -1,
        .description = "Quectel, EC600N-CN"
    },
    {
        .match_id = {(usb_dev_match_flags_t)0, 0, 0},
        .interface_idx = 0,
        .ep_addr = 0,
        .description = nullptr
    }
};

PppModemComponent::PppModemComponent() = default;

static void modem_init_task(void *arg) {
    char *apn_str = (char *) arg;
    const char *task_tag = "modem_init";
    
    ESP_LOGI(task_tag, "MODEM_INIT: Starting hardware power sequence for SIM7670G...");
    
    // Налаштування та ініціалізація апаратних пінів живлення (аналогічно до робочого коду)
    const gpio_config_t io_config = {
        .pin_bit_mask = (1ULL << GPIO_NUM_18),
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
    vTaskDelay(pdMS_TO_TICKS(50));
    gpio_set_level(GPIO_NUM_12, 1);

    ESP_LOGI(task_tag, "MODEM_INIT: Initializing USB Host CDC driver...");
    usbh_cdc_driver_config_t cdc_config = {
        .task_stack_size = 4096,
        .task_priority = 5,
        .task_coreid = 0,
        .skip_init_usb_host_driver = false,
    };
    esp_err_t err = usbh_cdc_driver_install(&cdc_config);
    if (err != ESP_OK) {
        ESP_LOGE(task_tag, "Failed to install USBH CDC driver: %s", esp_err_to_name(err));
        delete[] apn_str;
        vTaskDelete(nullptr);
        return;
    }

    ESP_LOGI(task_tag, "MODEM_INIT: Installing USBH modem board...");
    usbh_modem_config_t modem_config = {
        .modem_id_list = usb_modem_id_list,
        .at_tx_buffer_size = 256,
        .at_rx_buffer_size = 256,
        .pdp = {
            .enable = true,
            .cid = 1,
            .type = "IP",
            .apn = apn_str,
        },
    };
    
    usbh_modem_install(&modem_config);
    ESP_LOGI(task_tag, "MODEM_INIT: Modem successfully installed. Waiting for network and IP assignment...");

    // Головний цикл моніторингу підключення
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000));
        esp_netif_t *netif = usbh_modem_get_netif();
        if (netif != nullptr) {
            ESP_LOGI(task_tag, "MODEM_INIT: PPP Netif is active.");
        } else {
            ESP_LOGW(task_tag, "MODEM_INIT: Waiting for USB modem device enumeration...");
        }
    }

    delete[] apn_str;
    vTaskDelete(nullptr);
}

void PppModemComponent::setup() {
    ESP_LOGI(TAG, "PPP Modem Component setup initializing...");

    char *apn_copy = new char[this->apn_.length() + 1];
    strcpy(apn_copy, this->apn_.c_str());
    
    // Запуск таска ініціалізації модема
    xTaskCreate(modem_init_task, "modem_init_task", 4096, apn_copy, 4, nullptr);
}

void PppModemComponent::loop() {
}

void PppModemComponent::dump_config() {
    ESP_LOGCONFIG(TAG, "SIM7670G 4G Modem Component (USB Host):");
    ESP_LOGCONFIG(TAG, "  APN: %s", this->apn_.c_str());
}

}  // namespace ppp_modem
}  // namespace esphome
