#pragma once

#include <deque>
#include <vector>
#include "esphome/core/log.h"
#include "esphome/core/component.h"
#include "esphome/components/md5/md5.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/switch/switch.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/tuya_ble_tracker/common.h"
#include "esphome/components/tuya_ble_tracker/tuya_ble_tracker.h"

namespace esphome {
namespace tuya_ble_node {

using namespace esphome::tuya_ble;
using md5::MD5Digest;

struct DPSensorEntry {
  uint8_t dp_id;
  float scale;
  sensor::Sensor *sensor;
};

struct DPSwitchEntry {
  uint8_t dp_id;
  switch_::Switch *switch_obj;
};

class TuyaBLENode : public TYBLENode, public Component {
  
  std::deque<struct TYBLECommand> command_queue;
  std::vector<DPSensorEntry> dp_sensors_;
  std::vector<DPSwitchEntry> dp_switches_;
  binary_sensor::BinarySensor *connected_sensor_{nullptr};
  sensor::Sensor *rssi_sensor_{nullptr};
  public:
    bool has_command();

    bool has_session_key();

    void issue_command();

    void set_device_id(std::string device_id);

    void set_local_key(const char *local_key);

    void set_max_queued(uint8_t max);

    void set_uuid(std::string uuid);

    void pair();

    void request_info();

    void request_status();

    void reset_session_key();

    void toggle(bool value) override;

    void send_dp_bool(uint8_t dp_id, bool value) override;

    void add_dp_sensor(uint8_t dp_id, float scale, sensor::Sensor *s) {
      dp_sensors_.push_back({dp_id, scale, s});
    }

    void add_dp_switch(uint8_t dp_id, switch_::Switch *s) {
      dp_switches_.push_back({dp_id, s});
    }

    void set_connected_sensor(binary_sensor::BinarySensor *s) {
      this->connected_sensor_ = s;
    }

    void set_rssi_sensor(sensor::Sensor *s) {
      this->rssi_sensor_ = s;
    }
    int get_rssi() const { return this->rssi; }

    void on_rssi_updated(int rssi_val) override {
      this->rssi = rssi_val;
      if (this->rssi_sensor_ != nullptr && rssi_val != 0) {
        this->rssi_sensor_->publish_state(rssi_val);
      }
    }

    void on_connection_state(bool connected) override {
      this->is_paired = connected;
      if (this->connected_sensor_ != nullptr) {
        this->connected_sensor_->publish_state(connected);
      }
    }

    void on_dp_received(uint8_t dp_id, uint8_t type, uint16_t len, const unsigned char *value) override {
      if(len == 0) return;
      
      // Handle boolean switches (len == 1)
      for(auto &sw : dp_switches_) {
        if(sw.dp_id == dp_id) {
          sw.switch_obj->publish_state(value[0] != 0);
        }
      }

      // Handle numerical sensors (len up to 4)
      if(len <= 4) {
        int32_t raw = 0;
        for(uint16_t i = 0; i < len; i++) raw = (raw << 8) | value[i];
        for(auto &e : dp_sensors_) {
          if(e.dp_id == dp_id) e.sensor->publish_state(raw * e.scale);
        }
      }
    }

    void register_client(TYBLEClient *client) {
      ESP_LOGD("tuya_ble_node", "Client registered in node!");
      this->client = client;
      this->has_client = true;
    }

  protected:
    TYBLEClient *client;
    bool has_client = false;
    uint8_t max_queued = 1;

    void enqueue_command(TYBLECommand *command);
};

}  // namespace tuya_ble_node
}  // namespace esphome
