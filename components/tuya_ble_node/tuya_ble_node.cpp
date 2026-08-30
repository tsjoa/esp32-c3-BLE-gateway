#include "tuya_ble_node.h"

namespace esphome {
namespace tuya_ble_node {

static const char *const TAG = "tuya_ble_node";

void TuyaBLENode::enqueue_command(TYBLECommand *command) {
  
  while(this->command_queue.size() >= this->max_queued) {
    this->command_queue.pop_back();
  }

  this->command_queue.push_front(*command);
  
  ESP_LOGV(TAG, "enqueue_command: %s", binary_to_string(&command->data[0], command->data.size()).c_str());
}

bool TuyaBLENode::has_command() {
  return this->command_queue.size() > 0;
}

bool TuyaBLENode::has_session_key() {
  return !std::all_of(this->session_key, this->session_key + KEY_SIZE, [](unsigned char x) { return x == '\0'; });
}

void TuyaBLENode::issue_command() {
  if(!this->has_client) {
    ESP_LOGW(TAG, "No client registered at node");
    return;
  }

  if(!this->has_command()) {
    ESP_LOGW(TAG, "No commands to issue");
    return;
  }

  TYBLECommand *command = &this->command_queue.back();
  ESP_LOGI(TAG, "Issuing BLE Command code=0x%04X data=%s", (uint16_t)command->code, binary_to_string(&command->data[0], command->data.size()).c_str());
  this->client->write_data(command->code, &this->seq_num, &command->data[0], command->data.size(), command->key, command->response_to, command->protocol_version);
  this->command_queue.pop_back();
}

void TuyaBLENode::set_device_id(std::string device_id) {
  this->device_id = device_id;
}

void TuyaBLENode::set_local_key(const char *local_key) {

  memcpy(this->local_key, local_key, 6);

  MD5Digest *md5digest = new MD5Digest();
  
  md5digest->init();
  md5digest->add(local_key, 6);
  md5digest->calculate();
  md5digest->get_bytes(&this->login_key[0]);
  delete md5digest;
  
  ESP_LOGV(TAG, "Got local key (%s), turned into login key (%s)", local_key, binary_to_string(this->login_key, 16).c_str());
}

void TuyaBLENode::set_max_queued(uint8_t max) {
  this->max_queued = max;
}

void TuyaBLENode::set_uuid(std::string uuid) {
  this->uuid = uuid;
}

void TuyaBLENode::pair() {
  ESP_LOGD(TAG, "Pairing device...");

  size_t data_size = 44;
  size_t uuid_size = this->uuid.size();
  size_t device_id_size = this->device_id.size();
  unsigned char data[44]{0};

  if(device_id_size == 0 || uuid_size == 0) {
    ESP_LOGE(TAG, "Cannot pair if device_id and uuid are not set");
    return;
  }

  if(device_id_size + 6 + uuid_size > data_size) {
    ESP_LOGE(TAG, "Size of device_id + uuid is too big");
    return;
  }
  
  memcpy(data, this->uuid.c_str(), uuid_size);
  memcpy(&data[uuid_size], this->local_key, 6);
  memcpy(&data[uuid_size + 6], this->device_id.c_str(), device_id_size);

  this->client->write_data(TuyaBLECode::FUN_SENDER_PAIR, &this->seq_num, data, data_size, this->session_key);
}

void TuyaBLENode::request_info() {
  if(!this->has_session_key()) {
    ESP_LOGD(TAG, "Requesting device info...");
    unsigned char empty[1]{0};
    this->client->write_data(TuyaBLECode::FUN_SENDER_DEVICE_INFO, &this->seq_num, empty, 0, this->login_key, 0, 2);
  }
}

void TuyaBLENode::request_status() {
  ESP_LOGD(TAG, "Requesting device status...");
  unsigned char empty[1]{0};
  this->client->write_data(TuyaBLECode::FUN_SENDER_DEVICE_STATUS, &this->seq_num, empty, 0, this->session_key);
}

void TuyaBLENode::reset_session_key() {
  std::fill(this->session_key, this->session_key + KEY_SIZE, 0);
  this->is_paired = false;
  this->on_connection_state(false);
}

void TuyaBLENode::toggle(bool value) {
  this->send_dp_bool(0x14, value);
}

void TuyaBLENode::send_dp_bool(uint8_t dp_id, bool value) {
  if(!this->has_client) {
    ESP_LOGW(TAG, "No client registered at node");
    return;
  }

  ESP_LOGI(TAG, "Enqueue DP %d -> %s", dp_id, value ? "TRUE (ON)" : "FALSE (OFF)");

  TYBLECommand command = {
    TuyaBLECode::FUN_SENDER_DPS,
    { dp_id, 0x01, 0x01, (unsigned char)(value ? 1 : 0) },
    this->session_key,
    0,
    3,
  };

  this->enqueue_command(&command);
}

}  // namespace tuya_ble_node
}  // namespace esphome
