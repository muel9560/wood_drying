import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import spi
from esphome.const import CONF_ID

CODEOWNERS = ["@muel9560"]
DEPENDENCIES = ["spi"]
MULTI_CONF = True

ads1220_moisture_ns = cg.esphome_ns.namespace("ads1220_moisture")
ADS1220Moisture = ads1220_moisture_ns.class_(
    "ADS1220Moisture", cg.Component, spi.SPIDevice
)

CONF_DRDY_PIN = "drdy_pin"
CONF_R_REF = "r_ref"
CONF_IDAC = "idac"
CONF_LINE_FREQUENCY = "line_frequency"
CONF_MUX_PINS = "mux_pins"
CONF_MUX_ENABLE_PIN = "mux_enable_pin"
CONF_MUX_SETTLE_MS = "mux_settle_ms"

# IDAC current setting -> ADS1220 REG2 IDAC[2:0] code
IDAC_CURRENTS = {
    "10uA": 1,
    "50uA": 2,
    "100uA": 3,
    "250uA": 4,
    "500uA": 5,
    "1000uA": 6,
    "1500uA": 7,
}

# Mains rejection -> ADS1220 REG2 50/60[1:0] code. Rejecting a single frequency buys
# 105 dB against 90 dB for both, so pick the one you actually have.
LINE_FREQUENCIES = {
    "60Hz": 3,
    "50Hz": 2,
    "50/60Hz": 1,
    "off": 0,
}

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(ADS1220Moisture),
            cv.Optional(CONF_DRDY_PIN): pins.internal_gpio_input_pin_schema,
            # Full scale at gain 1 is exactly R_wood == r_ref, so this also sets the
            # dry-end ceiling: 200k reaches ~5.9% MC, 100k rails at ~8.2%.
            cv.Optional(CONF_R_REF, default=200000.0): cv.positive_float,
            cv.Optional(CONF_IDAC, default="10uA"): cv.enum(IDAC_CURRENTS, upper=False),
            cv.Optional(CONF_LINE_FREQUENCY, default="60Hz"): cv.enum(
                LINE_FREQUENCIES, upper=False
            ),
            cv.Optional(CONF_MUX_PINS): cv.All(
                [pins.gpio_output_pin_schema], cv.Length(min=4, max=4)
            ),
            cv.Optional(CONF_MUX_ENABLE_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_MUX_SETTLE_MS, default=50): cv.positive_int,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(spi.spi_device_schema(cs_pin_required=True))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await spi.register_spi_device(var, config)

    cg.add(var.set_r_ref(config[CONF_R_REF]))
    cg.add(var.set_idac_code(config[CONF_IDAC]))
    cg.add(var.set_line_freq_code(config[CONF_LINE_FREQUENCY]))
    cg.add(var.set_mux_settle_ms(config[CONF_MUX_SETTLE_MS]))

    if CONF_DRDY_PIN in config:
        drdy = await cg.gpio_pin_expression(config[CONF_DRDY_PIN])
        cg.add(var.set_drdy_pin(drdy))
    if CONF_MUX_PINS in config:
        for p in config[CONF_MUX_PINS]:
            mp = await cg.gpio_pin_expression(p)
            cg.add(var.add_mux_pin(mp))
    if CONF_MUX_ENABLE_PIN in config:
        me = await cg.gpio_pin_expression(config[CONF_MUX_ENABLE_PIN])
        cg.add(var.set_mux_enable_pin(me))
