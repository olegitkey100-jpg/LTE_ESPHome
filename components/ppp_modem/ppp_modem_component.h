#pragma once

#include "esphome/core/component.h"
#include <string>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

namespace esphome {
namespace ppp_modem {

class PppModemComponent : public Component {
 public:
  PppModemComponent();
  void setup() override;
  void loop() override;
  void dump_config() override;

  void set_apn(const std::string &apn) { this->apn_ = apn; }

 protected:
  std::string apn_{"internet"};
  EventGroupHandle_t event_group_{nullptr};
  void init_usb_pins_();
};

}  // namespace ppp_modem
}  // namespace esphome
