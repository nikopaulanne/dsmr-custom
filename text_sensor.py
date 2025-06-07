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
    """
    Generates C++ code to register standard DSMR text sensors with the dsmr_custom hub
    and defines preprocessor macros for the C++ layer to use with the vendored parser.
    """
    # Retrieve the already instantiated dsmr_custom hub object using its ID.
    hub = await cg.get_variable(config[CONF_DSMR_CUSTOM_HUB_ID])

    # List to store the C++ names of enabled standard text sensors for macro generation.
    active_standard_text_sensors_for_macro = []

    for key, conf_item in config.items():
        # Skip items that are not text_sensor configurations (e.g., the hub ID itself).
        if not isinstance(conf_item, dict) or key == CONF_DSMR_CUSTOM_HUB_ID:
            continue

        # If a text_sensor configuration for 'key' is present in the YAML:
        # 1. Create the C++ text_sensor object.
        #    text_sensor.new_text_sensor() handles creating the Pvariable and basic setup.
        var = await text_sensor.new_text_sensor(conf_item) # conf_item is the specific sensor's config

        # 2. Register this standard text_sensor with the hub by calling its C++ setter method.
        #    The setter method on the hub (e.g., hub.set_identification(var))
        #    is expected to match the 'key'.
        #    This also populates the standard_text_sensor_pointers_ map in C++
        #    which is used by the override mechanism.
        cg.add(getattr(hub, f"set_{key}")(var))

        # 3. Add the key (symbolic name) to the list for C++ macro generation,
        #    unless it's the special 'telegram' sensor, which might be handled
        #    differently or not included in the standard field parsing list of the vendored parser.
        #    The vendored parser (matthijskooijman/arduino-dsmr) does not typically have a
        #    'telegram' field in its ParsedData struct; the full telegram is handled separately.
        if key != "telegram": # Exclude 'telegram' from the macro list for parser fields
            active_standard_text_sensors_for_macro.append(f"F({key})")

    # Generate the DSMR_CUSTOM_TEXT_SENSOR_LIST C++ preprocessor macro.
    # This macro is consumed by the C++ MyData struct (based on the vendored parser)
    # and the publish_standard_sensors_() method in dsmr.cpp. It provides a list
    # of symbolic field names for standard text sensors.
    # DEVELOPER_NOTE_FOR_ESPHOME_DSMR_TEAM: This macro generation, similar to
    # DSMR_CUSTOM_SENSOR_LIST in sensor.py, allows the C++ parser to be templatized
    # with standard text fields. This aligns with ESPHome's architectural patterns.
    if active_standard_text_sensors_for_macro:
        cg.add_define(
            "DSMR_CUSTOM_TEXT_SENSOR_LIST(F, sep)", # Macro signature expected by C++
            cg.RawExpression(" sep ".join(active_standard_text_sensors_for_macro))
        )
    else:
        # Define as empty if no standard text sensors (excluding 'telegram') are configured.
        cg.add_define("DSMR_CUSTOM_TEXT_SENSOR_LIST(F, sep)", "")
