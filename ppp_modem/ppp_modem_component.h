#pragma once

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esp_netif.h"
#include "esp_netif_ppp.h"
#include "iot_usbh_modem.h"

namespace esphome {
namespace ppp_modem {

class PppModemComponent : public Component {
 public:
    PppModemComponent();
    void setup() override;
    void loop() override;
    void dump_config() override;

    // Методи конфігурації з YAML (опціонально для APN тощо)
    void set_apn(const std::string &apn) { apn_ = apn; }

 protected:
    std::string apn_{"internet"};
    EventGroupHandle_t event_group_{nullptr};
    
    void init_usb_pins_();
    void init_modem_hardware_();
};

}  // namespace ppp_modem
}  // namespace esphome
