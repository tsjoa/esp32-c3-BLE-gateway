#pragma once

#include "esphome/core/component.h"
#include "esphome/components/binary_sensor/binary_sensor.h"

namespace esphome {
namespace tuya_ble_node {

class TuyaBLEBinarySensor : public binary_sensor::BinarySensor, public Component {
};

}  // namespace tuya_ble_node
}  // namespace esphome
