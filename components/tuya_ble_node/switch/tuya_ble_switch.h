#pragma once

#include "esphome/core/component.h"
#include "esphome/components/switch/switch.h"
#include "esphome/components/tuya_ble_tracker/common.h"
#include "../tuya_ble_node.h"

namespace esphome {
namespace tuya_ble_node {

class TuyaBLESwitch : public switch_::Switch, public Component {
 public:
  void set_dp_id(uint8_t dp_id) { this->dp_id_ = dp_id; }
  void register_node(TuyaBLENode *node) {
    this->node_ = node;
    this->node_->add_dp_switch(this->dp_id_, this);
  }

 protected:
  void write_state(bool state) override {
    if (this->node_ != nullptr) {
      this->node_->send_dp_bool(this->dp_id_, state);
      this->publish_state(state);
    }
  }

  uint8_t dp_id_{0};
  TuyaBLENode *node_{nullptr};
};

}  // namespace tuya_ble_node
}  // namespace esphome
