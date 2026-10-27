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

"""
ESPHome platform for standard text-based DSMR sensors using the dsmr_custom hub.

This platform allows users to define standard DSMR text sensors (e.g., meter
identification, P1 version, timestamp, equipment IDs, messages) under their
main `text_sensor:` YAML configuration block. These sensors will be managed by
and retrieve data from a configured `dsmr_custom` hub instance.

Similar to the numeric sensor platform (sensor.py), this provides an alternative
or complementary way to define standard, well-known DSMR text sensors compared to
using the `custom_obis_sensors` list in the main `dsmr_custom:` hub configuration.

If an OBIS code for a standard text sensor defined here overlaps with an OBIS code
in `custom_obis_sensors` (in the main `dsmr_custom:` hub config), the custom
definition will take precedence due to the C++ override mechanism in the hub.
"""

import esphome.codegen as cg
import esphome.config_validation as cv
# Import the global text_sensor component module for text_sensor.text_sensor_schema.
from esphome.components import text_sensor
from esphome.const import (
    CONF_ID,         # Used internally by text_sensor.new_text_sensor
    CONF_INTERNAL,   # Option to mark a sensor as internal (not exposed to Home Assistant)
    # Other constants like CONF_ICON could be used in schema if needed.
)

# Import the Dsmr class (the hub) from the parent __init__.py.
from . import Dsmr as DsmrCustomHubClass # Alias for clarity
# Configuration key to link platform sensors to a specific dsmr_custom hub instance.
CONF_DSMR_CUSTOM_HUB_ID = "dsmr_custom_hub_id"

# Ensures the main 'dsmr_custom' hub component (defined in __init__.py)
# is loaded automatically when this text_sensor platform is used.
AUTO_LOAD = ["dsmr_custom"]

# Defines the YAML configuration schema for the dsmr_custom text_sensor platform.
# Users can enable and configure individual standard text DSMR sensors here.
CONFIG_SCHEMA = cv.Schema(
    {
        # Required: Link to an existing dsmr_custom hub component ID.
        cv.GenerateID(CONF_DSMR_CUSTOM_HUB_ID): cv.use_id(DsmrCustomHubClass),

        # Optional configuration for each standard text sensor.
        # These keys (e.g., "identification") become part of the C++
        # preprocessor macro DSMR_CUSTOM_TEXT_SENSOR_LIST.

        # P1 Telegram Header / Meter Identification
        # DEVELOPER_NOTE_FOR_ESPHOME_DSMR_TEAM: Raw P1 Identification Line -
        # This 'identification' text sensor, when enabled, typically captures the raw
        # first line of the P1 telegram (meter identification string) as provided by the
        # vendored parser. Exposing this is invaluable for users debugging P1 port issues,
        # verifying meter compatibility (e.g., with Finnish /ADN9... headers), and for
        # community sharing of configurations. The official DSMR component could benefit
        # from a similar standard text sensor for this purpose.
        cv.Optional("identification"): text_sensor.text_sensor_schema(
            # icon="mdi:barcode-scan", # Example icon
        ),
        # DSMR P1 Version Information
        cv.Optional("p1_version"): text_sensor.text_sensor_schema( # e.g., OBIS 1-3:0.2.8 for DSMR 2.2+
            # icon="mdi:tag-text-outline",
        ),
        cv.Optional("p1_version_be"): text_sensor.text_sensor_schema( # e.g., OBIS 0-0:96.1.4 for Belgian DSMR
            # icon="mdi:tag-text-outline",
        ),
        # Timestamp of the P1 Message
        cv.Optional("timestamp"): text_sensor.text_sensor_schema( # OBIS 0-0:1.0.0
            # icon="mdi:clock-outline",
        ),
        # Electricity Tariff Information
        cv.Optional("electricity_tariff"): text_sensor.text_sensor_schema( # OBIS 0-0:96.14.0
            # icon="mdi:theme-light-dark",
        ),
        # Electricity Failure Log (can be a multi-line string)
        cv.Optional("electricity_failure_log"): text_sensor.text_sensor_schema( # OBIS 1-0:99.97.0
            # icon="mdi:format-list-bulleted",
        ),
        # Messages from the Utility
        cv.Optional("message_short"): text_sensor.text_sensor_schema( # OBIS 0-0:96.13.1 (numeric code)
            # icon="mdi:message-text-outline",
        ),
        cv.Optional("message_long"): text_sensor.text_sensor_schema( # OBIS 0-0:96.13.0 (text message)
            # icon="mdi:message-text",
        ),
        # Equipment Identifiers for M-Bus devices (Gas, Water, Thermal, etc.)
        cv.Optional("gas_equipment_id"): text_sensor.text_sensor_schema( # e.g., OBIS 0-1:96.1.0
            # icon="mdi:identifier",
        ),
        cv.Optional("thermal_equipment_id"): text_sensor.text_sensor_schema( # e.g., OBIS 0-2:96.1.0
            # icon="mdi:identifier",
        ),
        cv.Optional("water_equipment_id"): text_sensor.text_sensor_schema( # e.g., OBIS 0-3:96.1.0
            # icon="mdi:identifier",
        ),
        cv.Optional("sub_equipment_id"): text_sensor.text_sensor_schema( # Generic slave device, e.g., OBIS 0-4:96.1.0
            # icon="mdi:identifier",
        ),
        # Some parsers might provide gas delivery as a text field including timestamp and unit.
        # While gas_delivered is usually numeric, a text version might exist in some parser fields.
        cv.Optional("gas_delivered_text"): text_sensor.text_sensor_schema(
            # icon="mdi:fire",
        ),
        # Full P1 Telegram (for debugging or advanced use cases)
        # This is often very long and primarily useful for diagnostics.
        # Defaulting to internal: true is a good practice.
        cv.Optional("telegram"): text_sensor.text_sensor_schema().extend(
            {cv.Optional(CONF_INTERNAL, default=True): cv.boolean}
            # icon="mdi:text-long",
        ),
        # Add other standard text sensors as needed, mirroring official DSMR component
        # capabilities or common fields from the matthijskooijman/arduino-dsmr parser library.
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_DSMR_CUSTOM_HUB_ID])
    for key, conf_item in config.items():
        if isinstance(conf_item, dict) and key != CONF_DSMR_CUSTOM_HUB_ID:
            var = await text_sensor.new_text_sensor(conf_item)
            cg.add(getattr(hub, f"set_{key}")(var))
