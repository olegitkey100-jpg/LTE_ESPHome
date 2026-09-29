#include "ppp_modem_component.h"
#include "esphome/core/log.h"
#include "esp_system.h"
#include "esp_check.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "iot_usbh_modem.h"

namespace esphome {
namespace ppp_modem {

static const char *TAG = "ppp_modem.component";

#define EVENT_GOT_IP_BIT (BIT0)

// Список сумісних 4G/3G модемів із повністю іменованими полями
static const usb_modem_id_t usb_modem_id_list[] = {
    {.match_id = {.flags = USB_DEVICE_ID_MATCH_VID_PID, .idVendor = 0x1782, .idProduct = 0x4d11, .bDeviceClass = 0, .bDeviceSubClass = 0, .bDeviceProtocol = 0, .bInterfaceClass = 0, .bInterfaceSubClass = 0, .bInterfaceProtocol = 0}, .itf_num = 2, .data_itf_num = -1, .name = "China Mobile, ML302/Fibocom, MC610-EU"},
    {.match_id = {.flags = USB_DEVICE_ID_MATCH_VID_PID, .idVendor = 0x1E0E, .idProduct = 0x9011, .bDeviceClass = 0, .bDeviceSubClass = 0, .bDeviceProtocol = 0, .bInterfaceClass = 0, .bInterfaceSubClass = 0, .bInterfaceProtocol = 0}, .itf_num = 5, .data_itf_num = -1, .name = "SIMCOM, A7600C1/SIMCOM, A7670E"},
    {.match_id = {.flags = USB_DEVICE_ID_MATCH_VID_PID, .idVendor = 0x1E0E, .idProduct = 0x9205, .bDeviceClass = 0, .bDeviceSubClass = 0, .bDeviceProtocol = 0, .bInterfaceClass = 0, .bInterfaceSubClass = 0, .bInterfaceProtocol = 0}, .itf_num = 2, .data_itf_num = -1, .name = "SIMCOM, SIM7080G"},
    {.match_id = {.flags = USB_DEVICE_ID_MATCH_VID_PID, .idVendor = 0x05C6, .idProduct = 0x9330, .bDeviceClass = 0, .bDeviceSubClass = 0, .bDeviceProtocol = 0, .bInterfaceClass = 0, .bInterfaceSubClass = 0, .bInterfaceProtocol = 0}, .itf_num = 2, .data_itf_num = -1, .name = "SIMCOM, SIM7670G-4G"},
    {.match_id = {.flags = USB_DEVICE_ID_MATCH_VID_PID, .idVendor = 0x2CB7, .idProduct = 0x0D01, .bDeviceClass = 0, .bDeviceSubClass = 0, .bDeviceProtocol = 0, .bInterfaceClass = 0, .bInterfaceSubClass = 0, .bInterfaceProtocol = 0}, .itf_num = 2, .data_itf_num = 6, .name = "Fibocom, LE270-CN"},
    {.match_id = {.flags = USB_DEVICE_ID_MATCH_VID_PID, .idVendor = 0x2C7C, .idProduct = 0x6001, .bDeviceClass = 0, .bDeviceSubClass = 0, .bDeviceProtocol = 0, .bInterfaceClass = 0, .bInterfaceSubClass = 0, .bInterfaceProtocol = 0}, .itf_num = 4, .data_itf_num = -1, .name = "Quectel, EC600N-CN"},
    {.match_id = {.flags = USB_DEVICE_ID_MATCH_VID_PID, .idVendor = 0x2C7C, .idProduct = 0x0125, .bDeviceClass = 0, .bDeviceSubClass = 0, .bDeviceProtocol = 0, .bInterfaceClass = 0, .bInterfaceSubClass = 0, .bInterfaceProtocol = 0}, .itf_num = 2, .data_itf_num = -1, .name = "Quectel, EC20"},
    {.match_id = {.flags = USB_DEVICE_ID_MATCH_VID_PID, .idVendor = 0x19D1, .idProduct = 0x1003, .bDeviceClass = 0, .bDeviceSubClass = 0, .bDeviceProtocol = 0, .bInterfaceClass = 0, .bInterfaceSubClass = 0, .bInterfaceProtocol = 0}, .itf_num = 2, .data_itf_num = -1, .name = "YUGE, YM310 X09"},
    {.match_id = {.flags = USB_DEVICE_ID_MATCH_VID_PID, .idVendor = 0x19D1, .idProduct = 0x0001, .bDeviceClass = 0, .bDeviceSubClass = 0, .bDeviceProtocol = 0, .bInterfaceClass = 0, .bInterfaceSubClass = 0, .bInterfaceProtocol = 0}, .itf_num = 2, .data_itf_num = -1, .name = "Luat, Air780E"},
    {.match_id = {static_cast<usb_dev_match_flags_t>(0)}},
};

// Обробник мережевих подій IP/PPP
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

    // Ініціалізація NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    // Мережевий стек
    esp_netif_init();

    this->event_group_ = xEventGroupCreate();
    if (this->event_group_ == nullptr) {
        ESP_LOGE(TAG, "Failed to create event group");
        this->mark_failed();
        return;
    }

    esp_event_handler_register(IP_EVENT, IP_EVENT_PPP_GOT_IP, ppp_event_handler, this->event_group_);
    esp_event_handler_register(IP_EVENT, IP_EVENT_PPP_LOST_IP, ppp_event_handler, this->event_group_);

    // Встановлення USB Host CDC драйвера
    usbh_cdc_driver_config_t config = {
        .task_stack_size = 1024 * 4,
        .task_priority = configMAX_PRIORITIES - 1,
        .task_coreid = 0,
        .skip_init_usb_host_driver = false,
    };
    usbh_cdc_driver_install(&config);

    // Конфігурація та встановлення модема
    usbh_modem_config_t modem_config = {
        .modem_id_list = usb_modem_id_list,
        .at_tx_buffer_size = 256,
        .at_rx_buffer_size = 256,
        .pdp = {
            .enable = true,
            .cid = 1,
            .type = "IP",
            .apn = this->apn_.c_str(),
        },
    };
    usbh_modem_install(&modem_config);
    ESP_LOGI(TAG, "Modem hardware installed successfully");
}

void PppModemComponent::loop() {
    // Неблокуюча перевірка чи встановлено з'єднання
    if (this->event_group_ != nullptr) {
        EventBits_t bits = xEventGroupGetBits(this->event_group_);
        if (bits & EVENT_GOT_IP_BIT) {
            // Модем підключено і має IP
        }
    }
}

void PppModemComponent::dump_config() {
    ESP_LOGCONFIG(TAG, "PPP 4G Modem Component:");
    ESP_LOGCONFIG(TAG, "  APN: %s", this->apn_.c_str());
}

}  // namespace ppp_modem
}  // namespace esphome
