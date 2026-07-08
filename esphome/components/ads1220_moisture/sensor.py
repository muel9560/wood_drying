import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import (
    CONF_ID,
    CONF_RESISTANCE,
    CONF_TEMPERATURE,
    DEVICE_CLASS_MOISTURE,
    STATE_CLASS_MEASUREMENT,
    UNIT_PERCENT,
)
from . import ADS1220Moisture, ads1220_moisture_ns

DEPENDENCIES = ["ads1220_moisture"]

CONF_ADS1220_MOISTURE_ID = "ads1220_moisture_id"
CONF_CHANNEL = "channel"
CONF_CAL_A = "cal_a"
CONF_CAL_B = "cal_b"
CONF_TEMPERATURE_BASELINE = "temperature_baseline"

MoistureChannel = ads1220_moisture_ns.class_(
    "MoistureChannel", cg.PollingComponent, sensor.Sensor
)

CONFIG_SCHEMA = (
    sensor.sensor_schema(
        MoistureChannel,
        unit_of_measurement=UNIT_PERCENT,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_MOISTURE,
        state_class=STATE_CLASS_MEASUREMENT,
        icon="mdi:water-percent",
    )
    .extend(
        {
            cv.GenerateID(CONF_ADS1220_MOISTURE_ID): cv.use_id(ADS1220Moisture),
            cv.Required(CONF_CHANNEL): cv.int_range(min=0, max=15),
            cv.Optional(CONF_CAL_A, default=45.8): cv.float_,
            cv.Optional(CONF_CAL_B, default=7.53): cv.float_,
            cv.Optional(CONF_TEMPERATURE_BASELINE, default=21.1): cv.float_,
            # Raw pre-clamp resistance (Ω) — keep it so MC is re-derivable downstream.
            cv.Optional(CONF_RESISTANCE): sensor.sensor_schema(
                unit_of_measurement="Ω",
                accuracy_decimals=0,
                state_class=STATE_CLASS_MEASUREMENT,
                icon="mdi:omega",
            ),
            # Ambient temperature source (°C) for the correction, e.g. a homeassistant sensor.
            cv.Optional(CONF_TEMPERATURE): cv.use_id(sensor.Sensor),
        }
    )
    .extend(cv.polling_component_schema("300s"))
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_ADS1220_MOISTURE_ID])
    var = cg.new_Pvariable(config[CONF_ID], parent)
    await cg.register_component(var, config)
    await sensor.register_sensor(var, config)

    cg.add(var.set_channel(config[CONF_CHANNEL]))
    cg.add(var.set_cal(config[CONF_CAL_A], config[CONF_CAL_B]))
    cg.add(var.set_temperature_baseline(config[CONF_TEMPERATURE_BASELINE]))

    if CONF_RESISTANCE in config:
        rs = await sensor.new_sensor(config[CONF_RESISTANCE])
        cg.add(var.set_resistance_sensor(rs))
    if CONF_TEMPERATURE in config:
        ts = await cg.get_variable(config[CONF_TEMPERATURE])
        cg.add(var.set_temperature_sensor(ts))
