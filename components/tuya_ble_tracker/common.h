#pragma once

#include <iostream>
#include <sstream>
#include <iomanip>
#include <vector>
#include <cstdint>
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"

namespace esphome {
namespace tuya_ble {

#define KEY_SIZE 0x10
#define IV_SIZE 0x10
#define AES_BLOCK_SIZE 0x10
#define META_SIZE 0x0C
#define CRC_SIZE 0x02
#define GATT_MTU 0x14

inline uint16_t tuya_crc16(const uint8_t *data, size_t length) {
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < length; i++) {
    crc ^= data[i] & 0xFF;
    for (int j = 0; j < 8; j++) {
      if (crc & 1) {
        crc = (crc >> 1) ^ 0xA001;
      } else {
        crc >>= 1;
      }
    }
  }
  return crc;
}

enum TuyaBLECode {
  FUN_SENDER_DEVICE_INFO = 0x0000,
  FUN_SENDER_PAIR = 0x0001,
  FUN_SENDER_DPS = 0x0002,
  FUN_SENDER_DEVICE_STATUS = 0x0003,

  FUN_SENDER_UNBIND = 0x0005,
  FUN_SENDER_DEVICE_RESET = 0x0006,

  FUN_SENDER_OTA_START = 0x000C,
  FUN_SENDER_OTA_FILE = 0x000D,
  FUN_SENDER_OTA_OFFSET = 0x000E,
  FUN_SENDER_OTA_UPGRADE = 0x000F,
  FUN_SENDER_OTA_OVER = 0x0010,

  FUN_SENDER_DPS_V4 = 0x0027,

  FUN_RECEIVE_DP = 0x8001,
  FUN_RECEIVE_TIME_DP = 0x8003,
  FUN_RECEIVE_SIGN_DP = 0x8004,
  FUN_RECEIVE_SIGN_TIME_DP = 0x8005,

  FUN_RECEIVE_DP_V4 = 0x8006,
  FUN_RECEIVE_TIME_DP_V4 = 0x8007,

  FUN_RECEIVE_TIME1_REQ = 0x8011,
  FUN_RECEIVE_TIME2_REQ = 0x8012
};

enum Security {
  AUTH_KEY = 0x01,
  LOGIN_KEY = 0x04,
  SESSION_KEY = 0x05,
};

struct TYBLECommand {
  TuyaBLECode code;
  std::vector<unsigned char> data;
  unsigned char *key;
  uint32_t response_to;
  int protocol_version;
};

class TYBLENode {
  public:
    std::string device_id;
    unsigned char local_key[6];
    unsigned char login_key[16];
    unsigned char session_key[16];
    std::string uuid;
    bool is_paired = false;
    uint32_t seq_num = 1;
    uint32_t last_detected = 0;
    int rssi = 0;

    virtual ~TYBLENode() = default;
    virtual bool has_command() = 0;
    virtual bool has_session_key() = 0;
    virtual void issue_command() = 0;
    virtual void pair() = 0;
    virtual void request_info() = 0;
    virtual void request_status() = 0;
    virtual void reset_session_key() = 0;
    virtual void toggle(bool value) = 0;
    virtual void send_dp_bool(uint8_t dp_id, bool value) = 0;
    virtual void on_dp_received(uint8_t dp_id, uint8_t type, uint16_t len, const unsigned char *value) {}
    virtual void on_connection_state(bool connected) {}
    virtual void on_rssi_updated(int rssi_val) {}
};

class TYBLEClient {
  public:
    virtual ~TYBLEClient() = default;
    virtual TYBLENode *get_node(uint64_t mac_address) = 0;
    virtual bool has_node(uint64_t mac_address) = 0;
    virtual void connect_mac_address(const uint64_t mac_address) = 0;
    virtual void set_address(uint64_t address) = 0;
    virtual bool connected() = 0;
    virtual void disconnect() = 0;
    virtual esp32_ble_tracker::ClientState state() const = 0;
    virtual void set_disconnect_callback(std::function<void()> &&f) = 0;
    virtual bool parse_device(const esp32_ble_tracker::ESPBTDevice &device) = 0;
    virtual void write_data(TuyaBLECode code, uint32_t *seq_num, unsigned char *data, size_t size, unsigned char *key, uint32_t response_to = 0, int protocol_version = 3) = 0;
};

  std::string binary_to_string(unsigned char *data, size_t size);

}  // namespace tuya_ble
}  // namespace esphome
