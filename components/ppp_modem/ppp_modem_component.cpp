#include "ppp_modem_component.h"
#include "esphome/core/log.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace esphome {
namespace ppp_modem {

static const char *TAG = "ppp_modem.component";

PppModemComponent::PppModemComponent() = default;

void PppModemComponent::setup() {
    ESP_LOGI(TAG, "PPP Modem Component setup called safely.");
    
    // Просто ініціалізуємо базові піни живлення і виходимо, ніяких важких тасків чи USB Host на старті
    const gpio_config_t power_io_config = {
        .pin_bit_mask = (1ULL << GPIO_NUM_17) | (1ULL << GPIO_NUM_12) | (1ULL << GPIO_NUM_13) | (1ULL << GPIO_NUM_18),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&power_io_config);
    gpio_set_level(GPIO_NUM_18, 1);
    gpio_set_level(GPIO_NUM_17, 1);
    gpio_set_level(GPIO_NUM_12, 1);
    gpio_set_level(GPIO_NUM_13, 0);
}

void PppModemComponent::loop() {
}

void PppModemComponent::dump_config() {
    ESP_LOGCONFIG(TAG, "SIM7670G 4G Modem Component:");
    ESP_LOGCONFIG(TAG, "  APN: %s", this->apn_.c_str());
}

}  // namespace ppp_modem
}  // namespace esphome
