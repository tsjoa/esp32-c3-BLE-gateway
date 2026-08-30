import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor
from esphome.components import tuya_ble_node
from esphome.const import CONF_ID

from .. import tuya_ble_node_ns

DEPENDENCIES = ["tuya_ble_node"]

TuyaBLEBinarySensor = tuya_ble_node_ns.class_(
    "TuyaBLEBinarySensor", binary_sensor.BinarySensor, cg.Component
)

CONFIG_SCHEMA = cv.All(
    binary_sensor.binary_sensor_schema(TuyaBLEBinarySensor)
    .extend(cv.COMPONENT_SCHEMA)
    .extend(tuya_ble_node.TUYA_BLE_NODE_SCHEMA)
)


async def to_code(config):
    var = await binary_sensor.new_binary_sensor(config)
    await cg.register_component(var, config)

    parent = await cg.get_variable(config[tuya_ble_node.CONF_TUYA_BLE_NODE_ID])
    cg.add(parent.set_connected_sensor(var))
