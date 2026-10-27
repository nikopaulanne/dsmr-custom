#
# This file is part of the dsmr_custom ESPHome component.
#
# This file is inspired by or based on the original ESPHome DSMR component,
# available at: https://github.com/esphome/esphome/tree/dev/esphome/components/dsmr
#
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.

"""dsmr_custom component for ESPHome."""

from pathlib import Path
from esphome import pins
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import uart
from esphome.components import sensor as esphome_global_sensor
from esphome.components import text_sensor as esphome_global_text_sensor

DEPENDENCIES = ["uart"]
AUTO_LOAD = ["sensor", "text_sensor"]

from esphome.const import (
    CONF_ID,
    CONF_UART_ID,
    CONF_NAME,
    CONF_PLATFORMIO_OPTIONS,
    CONF_RECEIVE_TIMEOUT,
)

DOMAIN = "dsmr_custom"
dsmr_custom_ns = cg.esphome_ns.namespace(DOMAIN)
Dsmr = dsmr_custom_ns.class_("Dsmr", cg.Component, uart.UARTDevice)
_sensitive = getattr(cv, "sensitive", lambda validator: validator)

COMPONENT_DIRECTORY = Path(__file__).parent.resolve()

CONF_DECRYPTION_KEY = "decryption_key"
CONF_REQUEST_PIN = "request_pin"
CONF_REQUEST_INTERVAL = "request_interval"
CONF_MAX_TELEGRAM_LENGTH = "max_telegram_length"
CONF_CRC_CHECK = "crc_check"
CONF_GAS_MBUS_ID = "gas_mbus_id"
CONF_WATER_MBUS_ID = "water_mbus_id"

CONF_CUSTOM_OBIS_SENSORS = "custom_obis_sensors"
CONF_OBIS_CODE = "code"
CONF_SENSOR_TYPE = "type"

def _validate_key(value):
    value = cv.string_strict(value)
    if not value:
        return ""
    if len(value) != 32 or any(char not in "0123456789abcdefABCDEF" for char in value):
        raise cv.Invalid("Decryption key must be exactly 32 hexadecimal characters (0-9, A-F).")
    return value.upper()


# Use the native schemas so IDs, filters and automations receive the same
# validation and code generation as ordinary ESPHome sensors.
CUSTOM_OBIS_SENSOR_SCHEMA = cv.typed_schema(
    {
        "sensor": esphome_global_sensor.sensor_schema().extend(
            {cv.Required(CONF_OBIS_CODE): cv.string_strict, cv.Required(CONF_NAME): cv.string_strict}
        ),
        "text_sensor": esphome_global_text_sensor.text_sensor_schema().extend(
            {cv.Required(CONF_OBIS_CODE): cv.string_strict, cv.Required(CONF_NAME): cv.string_strict}
        ),
    },
    key=CONF_SENSOR_TYPE,
    lower=True,
)


def _standard_fields(config, platform):
    return sorted({
        key
        for block in config.get(platform, [])
        if block.get("platform") == DOMAIN
        for key, value in block.items()
        if isinstance(value, dict) and key not in ("dsmr_custom_hub_id", "telegram")
    })


def _define_standard_fields(config):
    numeric = _standard_fields(config, "sensor")
    text = _standard_fields(config, "text_sensor")
    for name, fields in (("DSMR_CUSTOM_SENSOR_LIST", numeric), ("DSMR_CUSTOM_TEXT_SENSOR_LIST", text)):
        cg.add_define(f"{name}(F, sep)", cg.RawExpression(" sep ".join(f"F({field})" for field in fields)))
    cg.add_define("DSMR_CUSTOM_BOTH", cg.RawExpression("," if numeric and text else ""))


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(Dsmr),
        cv.Optional(CONF_MAX_TELEGRAM_LENGTH, default=1500): cv.int_range(min=1, max=65535),
        cv.Optional(CONF_DECRYPTION_KEY): _sensitive(_validate_key),
        cv.Optional(CONF_REQUEST_PIN): pins.gpio_output_pin_schema,
        cv.Optional(CONF_REQUEST_INTERVAL, default="0s"): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_RECEIVE_TIMEOUT, default="200ms"): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_CRC_CHECK, default=True): cv.boolean,
        cv.Optional(CONF_GAS_MBUS_ID, default=1): cv.int_range(min=0, max=255),
        cv.Optional(CONF_WATER_MBUS_ID, default=2): cv.int_range(min=0, max=255),
        cv.Optional(CONF_CUSTOM_OBIS_SENSORS): cv.ensure_list(CUSTOM_OBIS_SENSOR_SCHEMA),
        cv.Optional(CONF_PLATFORMIO_OPTIONS, default={}): cv.Schema(
            {
                cv.Optional("lib_deps"): cv.ensure_list(cv.string_strict),
            }
        ),
    }
).extend(uart.UART_DEVICE_SCHEMA).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    uart_var = await cg.get_variable(config[CONF_UART_ID])
    var = cg.new_Pvariable(config[CONF_ID], uart_var, config[CONF_CRC_CHECK])
    await cg.register_component(var, config)

    # Platform-specific crypto library configuration
    from esphome.core import CORE
    _define_standard_fields(CORE.config)
    if CORE.using_arduino:
        # Arduino: Use rweather/Crypto library
        cg.add_library("rweather/Crypto", "0.4.0")
    else:
        # ESP-IDF: Uses system MbedTLS - configure linker via extra_scripts
        # We need to manually link libmbedtls.a, libmbedcrypto.a, etc.
        # because ESPHome/PlatformIO doesn't automatically link them for custom components
        cg.add_platformio_option("extra_scripts", ["pre:" + (COMPONENT_DIRECTORY / "post_build.py").as_posix()])

    pio_options = config.get(CONF_PLATFORMIO_OPTIONS, {})
    lib_deps_yaml = pio_options.get("lib_deps", [])
    for dep in lib_deps_yaml:
        cg.add_library(dep, None)

    # Ensure the component's root directory itself is in includes
    # This helps resolve includes like #include "parser.h" if parser.h is in the same directory as dsmr.h
    cg.add_build_flag(f"-I{COMPONENT_DIRECTORY.resolve().as_posix()}")

    cg.add(var.set_max_telegram_length(config[CONF_MAX_TELEGRAM_LENGTH]))
    cg.add(var.set_receive_timeout(config[CONF_RECEIVE_TIMEOUT].total_milliseconds))

    if CONF_DECRYPTION_KEY in config:
         cg.add(var.set_decryption_key(config[CONF_DECRYPTION_KEY]))

    if CONF_REQUEST_PIN in config:
        request_pin_obj = await cg.gpio_pin_expression(config[CONF_REQUEST_PIN])
        cg.add(var.set_request_pin(request_pin_obj))

    cg.add(var.set_request_interval(config[CONF_REQUEST_INTERVAL].total_milliseconds))

    cg.add_build_flag(f"-DDSMR_CUSTOM_GAS_MBUS_ID={config[CONF_GAS_MBUS_ID]}")
    cg.add_build_flag(f"-DDSMR_CUSTOM_WATER_MBUS_ID={config[CONF_WATER_MBUS_ID]}")

    for conf_item in config.get(CONF_CUSTOM_OBIS_SENSORS, []):
        obis_code = conf_item[CONF_OBIS_CODE]
        if conf_item[CONF_SENSOR_TYPE] == "sensor":
            sens = await esphome_global_sensor.new_sensor(conf_item)
            cg.add(var.add_custom_numeric_sensor(obis_code, sens))
        else:
            sens = await esphome_global_text_sensor.new_text_sensor(conf_item)
            cg.add(var.add_custom_text_sensor(obis_code, sens))
