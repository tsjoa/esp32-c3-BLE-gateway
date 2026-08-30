import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.components import tuya_ble_node
from esphome.const import CONF_ID

DEPENDENCIES = ["tuya_ble_node"]

CONF_DP_ID = "dp_id"
CONF_SCALE = "scale"

CONFIG_SCHEMA = cv.All(
    sensor.sensor_schema().extend(
        {
            cv.Required(CONF_DP_ID): cv.int_range(min=1, max=255),
            cv.Optional(CONF_SCALE, default=1.0): cv.float_,
        }
    )
    .extend(tuya_ble_node.TUYA_BLE_NODE_SCHEMA)
)


async def to_code(config):
    sens = await sensor.new_sensor(config)

    node = await cg.get_variable(config[tuya_ble_node.CONF_TUYA_BLE_NODE_ID])
    cg.add(node.add_dp_sensor(config[CONF_DP_ID], config[CONF_SCALE], sens))