#pragma once

#include <vector>
#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/tuya_ble_tracker/common.h"

namespace esphome {
namespace tuya_ble {

struct DPSensorMapping {
  uint8_t dp_id;
  float scale;
  sensor::Sensor *sensor;
};

// Mixin added to TuyaBLENode via register_dp_sensor().
// When on_dp_received fires, we iterate registered mappings.
class TuyaBLESensorHub : public Component {
 public:
  void add_dp_sensor(uint8_t dp_id, float scale, sensor::Sensor *s) {
    mappings_.push_back({dp_id, scale, s});
  }

  void dispatch_dp(uint8_t dp_id, uint8_t type, uint16_t len, const unsigned char *value) {
    if(len == 0 || len > 4) return;
    int32_t raw = 0;
    for(uint16_t i = 0; i < len; i++)
      raw = (raw << 8) | value[i];
    for(auto &m : mappings_) {
      if(m.dp_id == dp_id)
        m.sensor->publish_state(raw * m.scale);
    }
  }

 protected:
  std::vector<DPSensorMapping> mappings_;
};

}  // namespace tuya_ble
}  // namespace esphome
