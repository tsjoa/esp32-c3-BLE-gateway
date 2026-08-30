#include "tuya_ble_client.h"
#include "esp_random.h"

namespace esphome {
namespace tuya_ble_client {

static const char *const TAG = "tuya_ble_client";

void TuyaBLEClient::set_state(esp32_ble_tracker::ClientState st) {
  esp32_ble_client::BLEClientBase::set_state(st);
  
  switch(st) {
    case esp32_ble_tracker::ClientState::INIT:
      ESP_LOGD(TAG, "INIT");
      break;
    case esp32_ble_tracker::ClientState::DISCONNECTING:
      ESP_LOGD(TAG, "DISCONNECTING");
      break;
    case esp32_ble_tracker::ClientState::IDLE:
      ESP_LOGD(TAG, "IDLE");
      break;
    case esp32_ble_tracker::ClientState::DISCOVERED:
      ESP_LOGD(TAG, "DISCOVERED");
      break;
    case esp32_ble_tracker::ClientState::CONNECTING:
      ESP_LOGD(TAG, "CONNECTING");
      break;
    case esp32_ble_tracker::ClientState::CONNECTED:
      ESP_LOGD(TAG, "CONNECTED");
      break;
    case esp32_ble_tracker::ClientState::ESTABLISHED:
      ESP_LOGD(TAG, "ESTABLISHED");
      break;
    default:
      ESP_LOGD(TAG, "Unknown state");
      break;
  }
}

void TuyaBLEClient::encrypt_data(uint32_t seq_num, TuyaBLECode code, unsigned char *data, size_t size, unsigned char *encrypted_data, size_t encrypted_size, unsigned char *key, unsigned char *iv, uint32_t response_to, uint8_t security_flag) {

  size_t inflated_size = META_SIZE + size + CRC_SIZE + (AES_BLOCK_SIZE - ((META_SIZE + size + CRC_SIZE) % AES_BLOCK_SIZE)); // 12 bytes of meta data + size of data + 2 bytes crc + padding
  unsigned char raw[inflated_size]{0};

  if(inflated_size + sizeof(security_flag) + IV_SIZE > encrypted_size) {
    ESP_LOGE(TAG, "Not enough space allocated for encrypted data!");
    return;
  }

  memcpy(&raw[12], data, size);

  raw[0] = (seq_num >> 24) & 0xff;
  raw[1] = (seq_num >> 16) & 0xff;
  raw[2] = (seq_num >>  8) & 0xff;
  raw[3] = (seq_num >>  0) & 0xff;

  raw[4] = (response_to >> 24) & 0xff;
  raw[5] = (response_to >> 16) & 0xff;
  raw[6] = (response_to >>  8) & 0xff;
  raw[7] = (response_to >>  0) & 0xff;

  raw[8] = (code >> 8) & 0xff;
  raw[9] = (code >> 0) & 0xff;

  raw[10] = (size >> 8) & 0xff;
  raw[11] = (size >> 0) & 0xff;

  uint16_t crc = tuya_crc16(raw, size + 12);

  raw[12 + size] = (crc >> 8) & 0xff;
  raw[13 + size] = (crc >> 0) & 0xff;
  
  ESP_LOGV(TAG, "%s", binary_to_string(raw, inflated_size).c_str());
  
  encrypted_data[0] = (unsigned char)security_flag;
  memcpy(&encrypted_data[1], iv, IV_SIZE);

  esp_aes_context aes;
  esp_aes_init(&aes);
  esp_aes_setkey(&aes, (const unsigned char *) key, KEY_SIZE * 8);
  esp_aes_crypt_cbc(&aes, ESP_AES_ENCRYPT, sizeof(raw), iv, (uint8_t*)raw, (uint8_t*)&encrypted_data[sizeof(security_flag) + IV_SIZE]);
  esp_aes_free(&aes);
}

std::tuple<uint32_t, TuyaBLECode, size_t, uint32_t> TuyaBLEClient::decrypt_data(unsigned char *encrypted_data, size_t encrypted_size, unsigned char *data, size_t size, unsigned char *key, unsigned char *iv) {

  if(size % AES_BLOCK_SIZE != 0) {
    ESP_LOGE(TAG, "Size of data to decrypt needs to be a multiple of block size");
    return std::make_tuple(0, TuyaBLECode::FUN_SENDER_DEVICE_INFO, 0, 0);
  }

  uint32_t seq_num;
  TuyaBLECode code;
  size_t decrypted_size;
  uint32_t response_to;

  esp_aes_context aes;
  esp_aes_init(&aes);
  esp_aes_setkey(&aes, (const unsigned char *)key, KEY_SIZE * 8);
  esp_aes_crypt_cbc(&aes, ESP_AES_DECRYPT, size, iv, (uint8_t*)encrypted_data, (uint8_t*)data);
  esp_aes_free(&aes);

  seq_num = (data[0] << 24) + (data[1] << 16) + (data[2] << 8) + data[3];
  response_to = (data[4] << 24) + (data[5] << 16) + (data[6] << 8) + data[7];
  code = (TuyaBLECode)((data[8] << 8) + data[9]);
  decrypted_size = (data[10] << 8) + data[11];

  ESP_LOGV(TAG, "%s", binary_to_string(data, size).c_str());

  return std::make_tuple(seq_num, code, decrypted_size, response_to);
}

void TuyaBLEClient::write_data(TuyaBLECode code, uint32_t *seq_num, unsigned char *data, size_t size, unsigned char *key, uint32_t response_to, int protocol_version) {
  if (this->write_char == nullptr) {
    ESP_LOGE(TAG, "Cannot write data: write_char is null");
    return;
  }

  size_t raw_meta_size = META_SIZE + size + CRC_SIZE;
  size_t padded_size = raw_meta_size + (AES_BLOCK_SIZE - (raw_meta_size % AES_BLOCK_SIZE));
  if (raw_meta_size % AES_BLOCK_SIZE == 0) {
    padded_size = raw_meta_size;
  }
  std::vector<unsigned char> raw(padded_size, 0);

  raw[0] = (*seq_num >> 24) & 0xff;
  raw[1] = (*seq_num >> 16) & 0xff;
  raw[2] = (*seq_num >>  8) & 0xff;
  raw[3] = (*seq_num >>  0) & 0xff;

  raw[4] = (response_to >> 24) & 0xff;
  raw[5] = (response_to >> 16) & 0xff;
  raw[6] = (response_to >>  8) & 0xff;
  raw[7] = (response_to >>  0) & 0xff;

  raw[8] = ((uint16_t)code >> 8) & 0xff;
  raw[9] = ((uint16_t)code >> 0) & 0xff;

  raw[10] = (size >> 8) & 0xff;
  raw[11] = (size >> 0) & 0xff;

  if (size > 0 && data != nullptr) {
    memcpy(&raw[12], data, size);
  }

  uint16_t crc = tuya_crc16(raw.data(), META_SIZE + size);
  raw[12 + size] = (crc >> 8) & 0xff;
  raw[13 + size] = (crc >> 0) & 0xff;

  uint8_t security_flag = (protocol_version == 2) ? Security::LOGIN_KEY : Security::SESSION_KEY;
  unsigned char iv[IV_SIZE]{0};
  esp_fill_random(iv, IV_SIZE);
  unsigned char iv_copy[IV_SIZE];
  memcpy(iv_copy, iv, IV_SIZE);

  std::vector<unsigned char> encrypted(1 + IV_SIZE + padded_size);
  encrypted[0] = security_flag;
  memcpy(&encrypted[1], iv, IV_SIZE);

  esp_aes_context aes;
  esp_aes_init(&aes);
  esp_aes_setkey(&aes, key, KEY_SIZE * 8);
  esp_aes_crypt_cbc(&aes, ESP_AES_ENCRYPT, padded_size, iv_copy, raw.data(), &encrypted[1 + IV_SIZE]);
  esp_aes_free(&aes);

  size_t total_encrypted_len = encrypted.size();
  size_t pos = 0;
  uint8_t packet_num = 0;

  while (pos < total_encrypted_len) {
    std::vector<unsigned char> packet;
    packet.push_back(packet_num);

    if (packet_num == 0) {
      packet.push_back((uint8_t)total_encrypted_len);
      packet.push_back((uint8_t)(protocol_version << 4));
    }

    size_t remaining_in_packet = GATT_MTU - packet.size();
    size_t chunk_size = std::min(remaining_in_packet, total_encrypted_len - pos);

    packet.insert(packet.end(), encrypted.begin() + pos, encrypted.begin() + pos + chunk_size);
    pos += chunk_size;

    ESP_LOGD(TAG, "Sending BLE packet %d (chunk=%d, total=%d)", packet_num, (int)chunk_size, (int)packet.size());
    this->write_char->write_value(packet.data(), packet.size(), ESP_GATT_WRITE_TYPE_NO_RSP);

    packet_num++;
  }

  (*seq_num)++;
}

void TuyaBLEClient::collect_data(unsigned char *data, size_t size) {
  if (size < 2) return;

  uint8_t packet_num = data[0];
  ESP_LOGV(TAG, "collect_data: pkt=%d len=%d", packet_num, (int)size);

  if (packet_num == 0) {
    this->data_collected.clear();
    this->data_collection_incrementor = 0;
    this->data_collection_state = DataCollectionState::COLLECTING;

    // Read expected length (variable byte length)
    size_t pos = 1;
    uint32_t expected_len = 0;
    uint8_t shift = 0;
    while (pos < size) {
      uint8_t byte = data[pos++];
      expected_len |= (byte & 0x7F) << shift;
      shift += 7;
      if ((byte & 0x80) == 0) break;
    }
    this->data_collection_expected_size = expected_len;
    pos++; // Skip protocol_version byte

    if (pos < size) {
      this->data_collected.insert(this->data_collected.end(), data + pos, data + size);
    }
  } else {
    if (this->data_collection_state != DataCollectionState::COLLECTING) return;
    this->data_collection_incrementor = packet_num;
    if (size > 1) {
      this->data_collected.insert(this->data_collected.end(), data + 1, data + size);
    }
  }

  ESP_LOGV(TAG, "Collected %d/%d bytes", (int)this->data_collected.size(), (int)this->data_collection_expected_size);

  if (this->data_collection_expected_size > 0 && this->data_collected.size() >= this->data_collection_expected_size) {
    ESP_LOGD(TAG, "Complete message collected (%d bytes)! Processing...", (int)this->data_collected.size());
    this->data_collection_state = DataCollectionState::COLLECTED;
  }
}

void TuyaBLEClient::process_data(TYBLENode *node) {
  if(this->data_collection_state != DataCollectionState::COLLECTED || this->data_collection_expected_size <= IV_SIZE + 1) { // Should be a multiple of 16 as well?
    ESP_LOGW(TAG, "Attempt to process received data aborted");
    return;
  }

  uint8_t security_flag = (uint8_t)this->data_collected[0];

  unsigned char *key;
  if(security_flag == Security::LOGIN_KEY) {
    key = node->login_key;
  }
  else if(security_flag == Security::AUTH_KEY) {
    //TODO: set auth key?
  }
  else {
    key = node->session_key;
  }
  
  unsigned char first_decrypted_part[AES_BLOCK_SIZE]{0};
  const size_t data_size_first_block = AES_BLOCK_SIZE - META_SIZE;
  size_t start_pos = sizeof(security_flag) + IV_SIZE;
  uint32_t seq_num;
  TuyaBLECode code;
  size_t decrypted_size;
  uint32_t response_to;

  // Decrypt first AES_BLOCK_SIZE (16 bytes), to get the meta data (Meta data is only 12 bytes long though, so there might be up to 4 bytes of data in this block as well):
  std::tie(seq_num, code, decrypted_size, response_to) = decrypt_data(&this->data_collected[start_pos], this->data_collection_expected_size - start_pos, first_decrypted_part, AES_BLOCK_SIZE, key, &this->data_collected[1]);
  // Use actual decrypted size for full payload decryption

  if(decrypted_size > 0) {
    size_t blocks = (decrypted_size - data_size_first_block) / AES_BLOCK_SIZE + ((decrypted_size - data_size_first_block) % AES_BLOCK_SIZE > 0); // Size needs to be a multiple of AES_BLOCK_SIZE
    if(decrypted_size <= data_size_first_block) {
      blocks = 0;
    }
    unsigned char decrypted_data[blocks * AES_BLOCK_SIZE + data_size_first_block]{0};

    memcpy(decrypted_data, &first_decrypted_part[META_SIZE], std::min((size_t)data_size_first_block, decrypted_size)); // Copy over existing data from first decrypted part (up to 4 bytes of the end)

    if(decrypted_size > data_size_first_block) { // if there's also data past the first decrypted part, add this as well
      decrypt_data(&this->data_collected[start_pos + AES_BLOCK_SIZE], this->data_collection_expected_size - start_pos - AES_BLOCK_SIZE, &decrypted_data[data_size_first_block], blocks * AES_BLOCK_SIZE, key, &this->data_collected[1]);
    }
  
    switch(code) {
      case TuyaBLECode::FUN_SENDER_DEVICE_INFO:
        {
          if(decrypted_size < 12) {
            ESP_LOGD(TAG, "DEVICE INFO response too short");
            return;
          }
          MD5Digest *md5digest = new MD5Digest();
    
          md5digest->init();
          md5digest->add(node->local_key, 6);
          md5digest->add(&decrypted_data[6], 6);
          md5digest->calculate();
          md5digest->get_bytes(&node->session_key[0]);
          ESP_LOGD(TAG, "Session key set!");

          ESP_LOGV(TAG, "%s", binary_to_string(node->session_key, KEY_SIZE).c_str());
        }
        break;

      case TuyaBLECode::FUN_SENDER_PAIR:
        {
          if(decrypted_size < 1) {
            ESP_LOGD(TAG, "PAIR response too short");
            return;
          }
          if(decrypted_data[0] == 0 || decrypted_data[0] == 2) { // Pair success or already paired
            node->is_paired = true;
            node->on_connection_state(true);
            ESP_LOGD(TAG, "Paired succesfully!");
            node->request_status();
          }
        }
        break;

      case TuyaBLECode::FUN_RECEIVE_TIME_DP:
      case TuyaBLECode::FUN_RECEIVE_SIGN_TIME_DP:
      case TuyaBLECode::FUN_RECEIVE_DP:
      case TuyaBLECode::FUN_RECEIVE_SIGN_DP:
      default:
        {
          ESP_LOGD(TAG, "Received code=0x%04X decrypted_size=%d", (uint16_t)code, decrypted_size);
          std::string hex = "";
          for(size_t i = 0; i < std::min(decrypted_size, (size_t)64); i++) {
            char buf[4]; snprintf(buf, 4, "%02X ", decrypted_data[i]); hex += buf;
          }
          ESP_LOGD(TAG, "Raw: %s", hex.c_str());

          size_t pos = 0;
          if (code == TuyaBLECode::FUN_RECEIVE_TIME_DP) {
            if (pos < decrypted_size) {
              uint8_t time_type = decrypted_data[pos++];
              pos += (time_type == 0) ? 13 : 4;
            }
          } else if (code == TuyaBLECode::FUN_RECEIVE_SIGN_TIME_DP) {
            pos += 3;
            if (pos < decrypted_size) {
              uint8_t time_type = decrypted_data[pos++];
              pos += (time_type == 0) ? 13 : 4;
            }
          } else if (code == TuyaBLECode::FUN_RECEIVE_SIGN_DP) {
            pos += 3;
          }

          while(pos + 3 <= decrypted_size) {
            uint8_t dp_id = decrypted_data[pos];
            uint8_t dp_type = decrypted_data[pos + 1];
            uint8_t dp_len = decrypted_data[pos + 2];
            pos += 3;
            if(pos + dp_len > decrypted_size) break;
            ESP_LOGD(TAG, "DP id=%d type=0x%02X len=%d", dp_id, dp_type, dp_len);
            node->on_dp_received(dp_id, dp_type, dp_len, &decrypted_data[pos]);
            pos += dp_len;
          }
        }
        break;
    }
  }

  this->data_collected.clear();
  this->data_collection_state = DataCollectionState::NO_DATA;
}

void TuyaBLEClient::register_for_notifications() {
  std::vector<std::string> service_uuids = {
    "0000a201-0000-1000-8000-00805f9b34fb",
    "0000fd50-0000-1000-8000-00805f9b34fb",
    "00001910-0000-1000-8000-00805f9b34fb"
  };

  this->notification_char = nullptr;
  this->write_char = nullptr;

  for (const auto &svc_uuid : service_uuids) {
    auto notify_c = this->get_characteristic(esp32_ble_tracker::ESPBTUUID::from_raw(svc_uuid), esp32_ble_tracker::ESPBTUUID::from_raw("00002b10-0000-1000-8000-00805f9b34fb"));
    auto write_c = this->get_characteristic(esp32_ble_tracker::ESPBTUUID::from_raw(svc_uuid), esp32_ble_tracker::ESPBTUUID::from_raw("00002b11-0000-1000-8000-00805f9b34fb"));
    if (notify_c != nullptr && write_c != nullptr) {
      ESP_LOGI(TAG, "Found Tuya GATT service: %s", svc_uuid.c_str());
      this->notification_char = notify_c;
      this->write_char = write_c;
      break;
    }
  }

  if (this->notification_char == nullptr || this->write_char == nullptr) {
    ESP_LOGE(TAG, "Required Tuya characteristics (0x2B10 / 0x2B11) not found in any service!");
    return;
  }

  ESP_LOGI(TAG, "Registering for notify on handle 0x%04X (write handle 0x%04X)", this->notification_char->handle, this->write_char->handle);
  esp_err_t status = this->register_for_notify(this->notification_char->handle);
  if(status != ESP_OK) {
    ESP_LOGW(TAG, "[%d] [%s] register_for_notify failed, status=%d", this->get_conn_id(), this->address_str_, status);
  }
}

void TuyaBLEClient::register_node(uint64_t mac_address, TYBLENode *tuyaBLENode) {

  if(mac_address == 0) {
    ESP_LOGE(TAG, "Attempted to register node with mac address 00:00:00:00:00:00");
    return;
  }

  this->nodes.insert(std::make_pair(mac_address, tuyaBLENode));
  
  ESP_LOGD(TAG, "Added: %llX from config", mac_address);
}

void TuyaBLEClient::set_disconnect_after(uint16_t disconnect_after) {

  this->disconnect_after = disconnect_after;
}

bool TuyaBLEClient::has_node(uint64_t mac_address) {
  return this->nodes.count(mac_address) > 0;
}

TYBLENode *TuyaBLEClient::get_node(uint64_t mac_address) {
  return this->nodes[mac_address];
}

void TuyaBLEClient::on_shutdown() {
}

bool TuyaBLEClient::gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param) {
  ESP_LOGV(TAG, "[%d] [%s] gattc_event_handler: event=%d gattc_if=%d", this->connection_index_, this->address_str_.c_str(), event, gattc_if);

  if (!esp32_ble_client::BLEClientBase::gattc_event_handler(event, gattc_if, param))
    return false;
  
  uint64_t mac_address = this->get_address();

  if(!this->has_node(mac_address)) {
    return true;
  }
  TYBLENode *node = this->get_node(mac_address);

  switch (event) {
    case ESP_GATTC_DISCONNECT_EVT: {
      ESP_LOGD(TAG, "Disconnected!");
      // Reset session so parse_device will reconnect on next advertisement
      node->reset_session_key();
      this->set_address(0);
      break;
    }
    case ESP_GATTC_SEARCH_CMPL_EVT: {
      if(esp32_ble_client::BLEClientBase::state() == esp32_ble_tracker::ClientState::ESTABLISHED) {
        this->register_for_notifications();
      }
      break;
    }
    case ESP_GATTC_REG_FOR_NOTIFY_EVT: {
      ESP_LOGD(TAG, "Notification channel ready! Requesting device info...");
      if(!node->has_session_key()) {
        this->should_disconnect = false;
        node->seq_num = 1;
        node->request_info();
      }
      break;
    }
    case ESP_GATTC_NOTIFY_EVT: {
      ESP_LOGV(TAG, "Notification received!");
      this->collect_data(param->notify.value, param->notify.value_len);

      if(this->data_collection_state == DataCollectionState::COLLECTED) {

        this->process_data(node);
        
        if(node->has_session_key()) {
          if(!node->is_paired && node->uuid.size() > 0 && node->device_id.size() > 0) {
            node->pair();
          }
          // Stay connected after pairing — device will push DP notifications
          else if(node->has_command()) {
            node->issue_command();
          }
        }
      }
      break;
    }
  }
  return true;
}

void TuyaBLEClient::connect_mac_address(const uint64_t mac_address) {
  ESP_LOGV(TAG, "Connecting to %llu", mac_address);

  TYBLENode *node = this->get_node(mac_address);

  this->set_address(mac_address);
  this->set_state(esp32_ble_tracker::ClientState::DISCOVERED);
  this->remote_bda_[0] = (mac_address >> 40) & 0xFF;
  this->remote_bda_[1] = (mac_address >> 32) & 0xFF;
  this->remote_bda_[2] = (mac_address >> 24) & 0xFF;
  this->remote_bda_[3] = (mac_address >> 16) & 0xFF;
  this->remote_bda_[4] = (mac_address >> 8) & 0xFF;
  this->remote_bda_[5] = (mac_address >> 0) & 0xFF;
  this->remote_addr_type_ = BLE_ADDR_TYPE_PUBLIC;

  node->reset_session_key(); // New session key every new connection?
}

void TuyaBLEClient::disconnect_when_appropriate() {
  this->should_disconnect = true;
  this->should_disconnect_timer = esphome::millis() + this->disconnect_after;
}

void TuyaBLEClient::disconnect_check() {
  ESP_LOGV(TAG, "disconnect_check. should_disconnect: %i, data_collection_state: %i, should_disconnect_timer: %i, millis: %i", this->should_disconnect, this->data_collection_state, this->should_disconnect_timer, esphome::millis());
  if(this->should_disconnect && this->data_collection_state == DataCollectionState::NO_DATA && esphome::millis() > this->should_disconnect_timer) {
    this->disconnect();
    this->should_disconnect = false;
  }
}

void TuyaBLEClient::set_disconnect_callback(std::function<void()> &&f) { this->disconnect_callback = std::move(f); }

void TuyaBLEClient::loop() {
  uint64_t address = this->get_address();

  if(address != 0) {
    if(this->has_node(address)) {
      TYBLENode *node = this->get_node(address);

      if(this->connected()) {
        static uint32_t last_rssi_poll = 0;
        if(esphome::millis() - last_rssi_poll > 3000) {
          last_rssi_poll = esphome::millis();
          esp_err_t err = esp_ble_gap_read_rssi(this->remote_bda_);
          if (err != ESP_OK) {
            ESP_LOGV(TAG, "esp_ble_gap_read_rssi error: %d", err);
          }
        }

        if(node->has_session_key() && node->has_command()) {
          node->issue_command();
        }
      }
    }
  }
  else if(this->nodes.size() > 0) {
    this->nodes_i = next(this->nodes_i);
    if(this->nodes_i == this->nodes.end()) {
      this->nodes_i = this->nodes.begin();
    }
    if(this->nodes_i->first != 0) {
      if(this->nodes_i->second->has_command()) {
        this->should_disconnect = false;
        this->connect_mac_address(this->nodes_i->first);
      }
    }
  }
  esp32_ble_client::BLEClientBase::loop();
}

}  // namespace tuya_ble_client
}  // namespace esphome