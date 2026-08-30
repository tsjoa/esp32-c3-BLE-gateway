#include "tuya_ble_tracker.h"

#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace tuya_ble_tracker {

static const char *const TAG = "tuya_ble_tracker";

bool TuyaBLETracker::parse_device(const esp32_ble_tracker::ESPBTDevice &device) {
  std::string addr_str = device.address_str();
  std::string name = device.get_name();
  int rssi = device.get_rssi();
  ESP_LOGI(TAG, "Saw BLE: %s (%s) RSSI: %d dBm", name.c_str(), addr_str.c_str(), rssi);

  if(!this->has_client) {
    ESP_LOGW(TAG, "No client registered!");
    return false;
  }

  uint64_t mac_address = device.address_uint64();
  bool is_tuya = false;
  for (const auto &mfg : device.get_manufacturer_datas()) {
    if (mfg.uuid == esp32_ble_tracker::ESPBTUUID::from_uint16(0x07D0)) {
      is_tuya = true;
      break;
    }
  }
  for (const auto &svc : device.get_service_uuids()) {
    if (svc == esp32_ble_tracker::ESPBTUUID::from_raw("0000a201-0000-1000-8000-00805f9b34fb") ||
        svc == esp32_ble_tracker::ESPBTUUID::from_raw("00001910-0000-1000-8000-00805f9b34fb") ||
        svc == esp32_ble_tracker::ESPBTUUID::from_uint16(0xFD50)) {
      is_tuya = true;
      break;
    }
  }

  if (is_tuya || this->client->has_node(mac_address)) {
    ESP_LOGI(TAG, "Discovered Tuya BLE device: %s (%s) RSSI: %d dBm", name.c_str(), addr_str.c_str(), rssi);
  }

  if(!this->client->has_node(mac_address)) {
    return false;
  }
  TYBLENode *ble_node = this->client->get_node(mac_address);
  ble_node->last_detected = esphome::millis();
  ble_node->on_rssi_updated(rssi);

  if(!ble_node->has_session_key()) {
    ESP_LOGI(TAG, "Connecting to Tuya BLE device %s (%s)...", name.c_str(), addr_str.c_str());
    this->client->connect_mac_address(mac_address);
    this->last_connection_attempt = esphome::millis();
  }

  return true;
}

void TuyaBLETracker::setup() {
  Component::setup();

  ESP_LOGD(TAG, "setup");

  if(this->has_client) {
    this->client->set_disconnect_callback([this]() { ESP_LOGD(TAG, "disconnected"); });
  }
}

void TuyaBLETracker::loop() {
  if(this->has_client) {
    //ESP_LOGD(TAG, "Connection state: %i, millis: %i, last_connection_attempt: %i, connected: %i", this->client->state(), esphome::millis(), this->last_connection_attempt, this->client->connected());
    if(this->client->state() == esp32_ble_tracker::ClientState::CONNECTING && esphome::millis() > this->last_connection_attempt + 20000) {
      if(!this->client->connected()) {
        ESP_LOGD(TAG, "Failed to connect");
        this->client->disconnect();
        this->client->set_address(0);
      }
    }
  }
}

}  // namespace tuya_ble
}  // namespace esphome