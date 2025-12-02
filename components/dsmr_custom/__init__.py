#
# This file is part of the dsmr_custom ESPHome component.
#
# This file is inspired by or based on the original ESPHome DSMR component,
# available at: https://github.com/esphome/esphome/tree/dev/esphome/components/dsmr
#
# Modifications and new code are Copyright (c) 2025 (Niko Paulanne).
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
CODEOWNERS = ["@nikopaulanne"]

from esphome.const import (
    CONF_ID,
    CONF_UART_ID,
    CONF_NAME,
    CONF_ICON,
    CONF_UNIT_OF_MEASUREMENT,
    CONF_ACCURACY_DECIMALS,
    CONF_DEVICE_CLASS,
    CONF_STATE_CLASS,
    CONF_DISABLED_BY_DEFAULT,
    CONF_ENTITY_CATEGORY,
    CONF_INTERNAL,
    CONF_FORCE_UPDATE,
    CONF_PLATFORMIO_OPTIONS,
    CONF_RECEIVE_TIMEOUT,
)

DOMAIN = "dsmr_custom"
dsmr_custom_ns = cg.esphome_ns.namespace(DOMAIN)
Dsmr = dsmr_custom_ns.class_("Dsmr", cg.Component, uart.UARTDevice)

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
    parts = [value[i : i + 2] for i in range(0, len(value), 2)]
    if len(parts) != 16:
        raise cv.Invalid(
            f"Decryption key must consist of 16 hexadecimal numbers (32 characters), got {len(value)} characters."
        )
    parts_int = []
    if any(len(part) != 2 for part in parts):
        raise cv.Invalid("Decryption key must be in format XXXXXX... (32 hex chars).")
    for part in parts:
        try:
            parts_int.append(int(part, 16))
        except ValueError:
            raise cv.Invalid(
                f"Decryption key part '{part}' is not a valid hexadecimal number."
            )
    return "".join(f"{part:02X}" for part in parts_int)


CUSTOM_OBIS_SENSOR_SCHEMA = cv.Schema({
    cv.Required(CONF_OBIS_CODE): cv.string_strict,
    cv.Required(CONF_NAME): cv.string_strict,
    cv.Required(CONF_SENSOR_TYPE): cv.enum({"sensor": "sensor", "text_sensor": "text_sensor"}, lower=True),
    cv.Optional(CONF_UNIT_OF_MEASUREMENT): cv.string_strict,
    cv.Optional(CONF_ACCURACY_DECIMALS): cv.positive_int,
    cv.Optional(CONF_DEVICE_CLASS): cv.string_strict,
    cv.Optional(CONF_STATE_CLASS): cv.enum(esphome_global_sensor.STATE_CLASSES, lower=True),
    cv.Optional(CONF_ICON): cv.icon,
    cv.Optional(CONF_FORCE_UPDATE, default=False): cv.boolean,
    cv.Optional(CONF_DISABLED_BY_DEFAULT, default=False): cv.boolean,
    cv.Optional(CONF_ENTITY_CATEGORY, default=""): cv.entity_category,
    cv.Optional(CONF_INTERNAL, default=False): cv.boolean,
})

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(Dsmr),
        cv.Optional(CONF_MAX_TELEGRAM_LENGTH, default=1500): cv.positive_int,
        cv.Optional(CONF_DECRYPTION_KEY): _validate_key,
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

    cg.add_library("rweather/Crypto", "0.4.0")

    pio_options = config.get(CONF_PLATFORMIO_OPTIONS, {})
    lib_deps_yaml = pio_options.get("lib_deps", [])
    for dep in lib_deps_yaml:
        cg.add_library(dep, None)

    # Ensure the component's root directory itself is in includes
    # This helps resolve includes like #include "parser.h" if parser.h is in the same directory as dsmr.h
    cg.add_build_flag(f"-I{str(COMPONENT_DIRECTORY)}")

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

    if CONF_CUSTOM_OBIS_SENSORS in config:
        for i, conf_item in enumerate(config[CONF_CUSTOM_OBIS_SENSORS]):
            obis_code = conf_item[CONF_OBIS_CODE]
            sensor_name = conf_item[CONF_NAME]
            sensor_type_str = conf_item[CONF_SENSOR_TYPE]
            disabled_by_default = conf_item[CONF_DISABLED_BY_DEFAULT]
            entity_category = conf_item[CONF_ENTITY_CATEGORY]
            internal = conf_item[CONF_INTERNAL]

            sanitized_obis = obis_code.replace(':', '_').replace('.', '_').replace('-', '_')
            sensor_id_str = f"{config[CONF_ID].id}_custom_{sanitized_obis}_{i}"

            if sensor_type_str == "sensor":
                force_update = conf_item[CONF_FORCE_UPDATE]
                sensor_id_obj = cv.declare_id(esphome_global_sensor.Sensor)(sensor_id_str)
                sensor_config_payload = {
                    CONF_ID: sensor_id_obj,
                    CONF_NAME: sensor_name,
                    CONF_DISABLED_BY_DEFAULT: disabled_by_default,
                    CONF_ENTITY_CATEGORY: entity_category,
                    CONF_INTERNAL: internal,
                    CONF_FORCE_UPDATE: force_update,
                }
                if CONF_UNIT_OF_MEASUREMENT in conf_item:
                    sensor_config_payload[CONF_UNIT_OF_MEASUREMENT] = conf_item[CONF_UNIT_OF_MEASUREMENT]
                if CONF_ACCURACY_DECIMALS in conf_item:
                    sensor_config_payload[CONF_ACCURACY_DECIMALS] = conf_item[CONF_ACCURACY_DECIMALS]
                if CONF_DEVICE_CLASS in conf_item:
                    sensor_config_payload[CONF_DEVICE_CLASS] = conf_item[CONF_DEVICE_CLASS]
                if CONF_STATE_CLASS in conf_item:
                    sensor_config_payload[CONF_STATE_CLASS] = conf_item[CONF_STATE_CLASS]
                if CONF_ICON in conf_item:
                    sensor_config_payload[CONF_ICON] = conf_item[CONF_ICON]

                sens = await esphome_global_sensor.new_sensor(sensor_config_payload)
                cg.add(var.add_custom_numeric_sensor(obis_code, sens))

            elif sensor_type_str == "text_sensor":
                sensor_id_obj = cv.declare_id(esphome_global_text_sensor.TextSensor)(sensor_id_str)
                text_sensor_config_payload = {
                    CONF_ID: sensor_id_obj,
                    CONF_NAME: sensor_name,
                    CONF_DISABLED_BY_DEFAULT: disabled_by_default,
                    CONF_ENTITY_CATEGORY: entity_category,
                    CONF_INTERNAL: internal,
                }
                if CONF_ICON in conf_item:
                    text_sensor_config_payload[CONF_ICON] = conf_item[CONF_ICON]

                text_sens = await esphome_global_text_sensor.new_text_sensor(text_sensor_config_payload)
                cg.add(var.add_custom_text_sensor(obis_code, text_sens))
