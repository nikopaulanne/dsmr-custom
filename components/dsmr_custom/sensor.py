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
ESPHome platform for standard numeric DSMR sensors using the dsmr_custom hub.

This platform allows users to define standard DSMR numeric sensors (e.g.,
energy delivered/returned tariffs, power, voltage, current, gas/water delivery)
under their main `sensor:` YAML configuration block. These sensors will be
managed by and retrieve data from a configured `dsmr_custom` hub instance.

The primary method for defining sensors in the `dsmr_custom` component is via
the `custom_obis_sensors` list in the main `dsmr_custom:` hub configuration.
However, this platform provides an alternative or complementary way for users
who prefer to define standard, well-known DSMR sensors in the global `sensor:` block.

If an OBIS code for a standard sensor defined here overlaps with an OBIS code
in `custom_obis_sensors` (in the main `dsmr_custom:` hub config), the custom
definition will take precedence due to the C++ override mechanism in the hub.
"""

import esphome.codegen as cg
import esphome.config_validation as cv
# Import the global sensor component module for sensor.sensor_schema.
from esphome.components import sensor
from esphome.const import (
    CONF_ID, # Used internally by sensor.new_sensor
    # Device Classes for sensors
    DEVICE_CLASS_CURRENT,
    DEVICE_CLASS_ENERGY,
    DEVICE_CLASS_GAS,
    DEVICE_CLASS_POWER,
    DEVICE_CLASS_VOLTAGE,
    DEVICE_CLASS_WATER, # Added as per typical DSMR sensors
    # State Classes for sensors
    STATE_CLASS_MEASUREMENT,
    STATE_CLASS_TOTAL_INCREASING,
    # Units of Measurement
    UNIT_AMPERE,
    UNIT_CUBIC_METER,
    UNIT_KILOVOLT_AMPS_REACTIVE, # kvar for reactive power
    UNIT_KILOVOLT_AMPS_REACTIVE_HOURS, # kvarh for reactive energy
    UNIT_KILOWATT,
    UNIT_KILOWATT_HOURS,
    UNIT_VOLT,
    # Other constants that might be used for icons, etc.
    # ICON_FLASH, ICON_COUNTER, etc. are typically not imported directly
    # but used as string literals in schema definitions.
)

# Import the Dsmr class (the hub) from the parent __init__.py.
# The '.' means current package.
from . import Dsmr as DsmrCustomHubClass # Alias for clarity
# Configuration key to link platform sensors to a specific dsmr_custom hub instance.
CONF_DSMR_CUSTOM_HUB_ID = "dsmr_custom_hub_id"

# Ensures the main 'dsmr_custom' hub component (defined in __init__.py)
# is loaded automatically when this sensor platform is used. This makes the
# hub instance available for these standard sensors to register with.
# This is a standard pattern for custom components that provide a hub and platforms.
AUTO_LOAD = ["dsmr_custom"]

# Defines the YAML configuration schema for the dsmr_custom sensor platform.
# Users can enable and configure individual standard numeric DSMR sensors here.
CONFIG_SCHEMA = cv.Schema(
    {
        # Required: Link to an existing dsmr_custom hub component ID.
        # All sensors defined here will be associated with this hub.
        cv.GenerateID(CONF_DSMR_CUSTOM_HUB_ID): cv.use_id(DsmrCustomHubClass),

        # Optional configuration for each standard numeric sensor.
        # These follow the naming convention from the matthijskooijman/arduino-dsmr library
        # or common DSMR standards. The keys here (e.g., "energy_delivered_lux")
        # become part of the C++ preprocessor macro DSMR_CUSTOM_SENSOR_LIST.

        # --- Energy Delivered/Returned ---
        cv.Optional("energy_delivered_lux"): sensor.sensor_schema(
            unit_of_measurement=UNIT_KILOWATT_HOURS,
            accuracy_decimals=3,
            device_class=DEVICE_CLASS_ENERGY,
            state_class=STATE_CLASS_TOTAL_INCREASING,
            # icon="mdi:home-import-outline", # Example icon
        ),
        cv.Optional("energy_delivered_tariff1"): sensor.sensor_schema(
            unit_of_measurement=UNIT_KILOWATT_HOURS,
            accuracy_decimals=3,
            device_class=DEVICE_CLASS_ENERGY,
            state_class=STATE_CLASS_TOTAL_INCREASING,
            # icon="mdi:counter",
        ),
        cv.Optional("energy_delivered_tariff2"): sensor.sensor_schema(
            unit_of_measurement=UNIT_KILOWATT_HOURS,
            accuracy_decimals=3,
            device_class=DEVICE_CLASS_ENERGY,
            state_class=STATE_CLASS_TOTAL_INCREASING,
            # icon="mdi:counter",
        ),
        cv.Optional("energy_returned_lux"): sensor.sensor_schema( # Corresponds to OBIS 1-0:2.8.0
            unit_of_measurement=UNIT_KILOWATT_HOURS,
            accuracy_decimals=3,
            device_class=DEVICE_CLASS_ENERGY,
            state_class=STATE_CLASS_TOTAL_INCREASING,
            # icon="mdi:home-export-outline",
        ),
        cv.Optional("energy_returned_tariff1"): sensor.sensor_schema(
            unit_of_measurement=UNIT_KILOWATT_HOURS,
            accuracy_decimals=3,
            device_class=DEVICE_CLASS_ENERGY,
            state_class=STATE_CLASS_TOTAL_INCREASING,
            # icon="mdi:counter",
        ),
        cv.Optional("energy_returned_tariff2"): sensor.sensor_schema(
            unit_of_measurement=UNIT_KILOWATT_HOURS,
            accuracy_decimals=3,
            device_class=DEVICE_CLASS_ENERGY,
            state_class=STATE_CLASS_TOTAL_INCREASING,
            # icon="mdi:counter",
        ),
        # Reactive Energy (example, if supported by standard parser fields)
        cv.Optional("total_imported_energy"): sensor.sensor_schema( # Typically reactive import, e.g., 1-0:3.8.0
            unit_of_measurement=UNIT_KILOVOLT_AMPS_REACTIVE_HOURS, # kvarh
            accuracy_decimals=3,
            device_class=DEVICE_CLASS_ENERGY, # Or a more specific reactive energy class if available
            state_class=STATE_CLASS_TOTAL_INCREASING,
            # icon="mdi:sine-wave",
        ),
        cv.Optional("total_exported_energy"): sensor.sensor_schema( # Typically reactive export, e.g., 1-0:4.8.0
            unit_of_measurement=UNIT_KILOVOLT_AMPS_REACTIVE_HOURS, # kvarh
            accuracy_decimals=3,
            device_class=DEVICE_CLASS_ENERGY,
            state_class=STATE_CLASS_TOTAL_INCREASING,
            # icon="mdi:sine-wave",
        ),

        # --- Power ---
        cv.Optional("power_delivered"): sensor.sensor_schema( # Total active power import (+P)
            unit_of_measurement=UNIT_KILOWATT, # Or UNIT_WATT if parser provides W
            accuracy_decimals=3,
            device_class=DEVICE_CLASS_POWER,
            state_class=STATE_CLASS_MEASUREMENT,
            # icon="mdi:flash",
        ),
        cv.Optional("power_returned"): sensor.sensor_schema( # Total active power export (-P)
            unit_of_measurement=UNIT_KILOWATT,
            accuracy_decimals=3,
            device_class=DEVICE_CLASS_POWER,
            state_class=STATE_CLASS_MEASUREMENT,
            # icon="mdi:flash-outline",
        ),
        # Reactive Power
        cv.Optional("reactive_power_delivered"): sensor.sensor_schema( # Total reactive power import (+Q)
            unit_of_measurement=UNIT_KILOVOLT_AMPS_REACTIVE, # kvar
            accuracy_decimals=3,
            state_class=STATE_CLASS_MEASUREMENT,
            # icon="mdi:sine-wave", # Consider specific icon for reactive power
        ),
        cv.Optional("reactive_power_returned"): sensor.sensor_schema( # Total reactive power export (-Q)
            unit_of_measurement=UNIT_KILOVOLT_AMPS_REACTIVE, # kvar
            accuracy_decimals=3,
            state_class=STATE_CLASS_MEASUREMENT,
            # icon="mdi:sine-wave",
        ),

        # --- Electricity Status & Failures ---
        cv.Optional("electricity_threshold"): sensor.sensor_schema( # Not common in modern DSMR
            accuracy_decimals=3, # Unit depends on specific OBIS code definition
        ),
        cv.Optional("electricity_switch_position"): sensor.sensor_schema( # Not common
            accuracy_decimals=0, # Typically an integer/enum
        ),
        cv.Optional("electricity_failures"): sensor.sensor_schema( # Number of power failures
            accuracy_decimals=0,
            # icon="mdi:alert-circle-outline",
        ),
        cv.Optional("electricity_long_failures"): sensor.sensor_schema( # Number of long power failures
            accuracy_decimals=0,
            # icon="mdi:alert-outline",
        ),
        cv.Optional("electricity_sags_l1"): sensor.sensor_schema(accuracy_decimals=0),
        cv.Optional("electricity_sags_l2"): sensor.sensor_schema(accuracy_decimals=0),
        cv.Optional("electricity_sags_l3"): sensor.sensor_schema(accuracy_decimals=0),
        cv.Optional("electricity_swells_l1"): sensor.sensor_schema(accuracy_decimals=0),
        cv.Optional("electricity_swells_l2"): sensor.sensor_schema(accuracy_decimals=0),
        cv.Optional("electricity_swells_l3"): sensor.sensor_schema(accuracy_decimals=0),

        # --- Phase-Specific Current ---
        cv.Optional("current_l1"): sensor.sensor_schema(
            unit_of_measurement=UNIT_AMPERE,
            accuracy_decimals=1, # Or 2, or 3 depending on meter/parser
            device_class=DEVICE_CLASS_CURRENT,
            state_class=STATE_CLASS_MEASUREMENT,
            # icon="mdi:current-ac",
        ),
        cv.Optional("current_l2"): sensor.sensor_schema(
            unit_of_measurement=UNIT_AMPERE,
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_CURRENT,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional("current_l3"): sensor.sensor_schema(
            unit_of_measurement=UNIT_AMPERE,
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_CURRENT,
            state_class=STATE_CLASS_MEASUREMENT,
        ),

        # --- Phase-Specific Active Power ---
        cv.Optional("power_delivered_l1"): sensor.sensor_schema(
            unit_of_measurement=UNIT_KILOWATT,
            accuracy_decimals=3,
            device_class=DEVICE_CLASS_POWER,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional("power_delivered_l2"): sensor.sensor_schema(
            unit_of_measurement=UNIT_KILOWATT,
            accuracy_decimals=3,
            device_class=DEVICE_CLASS_POWER,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional("power_delivered_l3"): sensor.sensor_schema(
            unit_of_measurement=UNIT_KILOWATT,
            accuracy_decimals=3,
            device_class=DEVICE_CLASS_POWER,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional("power_returned_l1"): sensor.sensor_schema(
            unit_of_measurement=UNIT_KILOWATT,
            accuracy_decimals=3,
            device_class=DEVICE_CLASS_POWER,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional("power_returned_l2"): sensor.sensor_schema(
            unit_of_measurement=UNIT_KILOWATT,
            accuracy_decimals=3,
            device_class=DEVICE_CLASS_POWER,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional("power_returned_l3"): sensor.sensor_schema(
            unit_of_measurement=UNIT_KILOWATT,
            accuracy_decimals=3,
            device_class=DEVICE_CLASS_POWER,
            state_class=STATE_CLASS_MEASUREMENT,
        ),

        # --- Phase-Specific Reactive Power (Example) ---
        cv.Optional("reactive_power_delivered_l1"): sensor.sensor_schema(
            unit_of_measurement=UNIT_KILOVOLT_AMPS_REACTIVE, # kvar
            accuracy_decimals=3,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional("reactive_power_delivered_l2"): sensor.sensor_schema(
            unit_of_measurement=UNIT_KILOVOLT_AMPS_REACTIVE,
            accuracy_decimals=3,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional("reactive_power_delivered_l3"): sensor.sensor_schema(
            unit_of_measurement=UNIT_KILOVOLT_AMPS_REACTIVE,
            accuracy_decimals=3,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional("reactive_power_returned_l1"): sensor.sensor_schema(
            unit_of_measurement=UNIT_KILOVOLT_AMPS_REACTIVE,
            accuracy_decimals=3,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional("reactive_power_returned_l2"): sensor.sensor_schema(
            unit_of_measurement=UNIT_KILOVOLT_AMPS_REACTIVE,
            accuracy_decimals=3,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional("reactive_power_returned_l3"): sensor.sensor_schema(
            unit_of_measurement=UNIT_KILOVOLT_AMPS_REACTIVE,
            accuracy_decimals=3,
            state_class=STATE_CLASS_MEASUREMENT,
        ),

        # --- Phase-Specific Voltage ---
        cv.Optional("voltage_l1"): sensor.sensor_schema(
            unit_of_measurement=UNIT_VOLT,
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_VOLTAGE,
            state_class=STATE_CLASS_MEASUREMENT,
            # icon="mdi:lightning-bolt",
        ),
        cv.Optional("voltage_l2"): sensor.sensor_schema(
            unit_of_measurement=UNIT_VOLT,
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_VOLTAGE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional("voltage_l3"): sensor.sensor_schema(
            unit_of_measurement=UNIT_VOLT,
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_VOLTAGE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),

        # --- Other Meter Types (Gas, Water, etc.) ---
        cv.Optional("gas_delivered"): sensor.sensor_schema( # For Dutch meters
            unit_of_measurement=UNIT_CUBIC_METER,
            accuracy_decimals=3,
            device_class=DEVICE_CLASS_GAS,
            state_class=STATE_CLASS_TOTAL_INCREASING,
            # icon="mdi:fire",
        ),
        cv.Optional("gas_delivered_be"): sensor.sensor_schema( # For Belgian meters
            unit_of_measurement=UNIT_CUBIC_METER,
            accuracy_decimals=3,
            device_class=DEVICE_CLASS_GAS,
            state_class=STATE_CLASS_TOTAL_INCREASING,
            # icon="mdi:fire",
        ),
        cv.Optional("water_delivered"): sensor.sensor_schema(
            unit_of_measurement=UNIT_CUBIC_METER,
            accuracy_decimals=3,
            device_class=DEVICE_CLASS_WATER,
            state_class=STATE_CLASS_TOTAL_INCREASING,
            # icon="mdi:water",
        ),
        # --- SESKO / Finnish specific or other advanced sensors (examples) ---
        # These are often defined via custom_obis_sensors, but could be standard if widely used
        # and supported by the vendored parser's fields.h.
        # Example: Maximum Demand
        cv.Optional("active_energy_import_current_average_demand"): sensor.sensor_schema( # e.g. OBIS 1-0:1.4.0
            unit_of_measurement=UNIT_KILOWATT,
            accuracy_decimals=3,
            device_class=DEVICE_CLASS_POWER,
            state_class=STATE_CLASS_MEASUREMENT,
            # icon="mdi:chart-bell-curve",
        ),
        cv.Optional("active_energy_import_maximum_demand_running_month"): sensor.sensor_schema( # e.g. OBIS 1-0:1.6.0
            unit_of_measurement=UNIT_KILOWATT,
            accuracy_decimals=3,
            device_class=DEVICE_CLASS_POWER,
            state_class=STATE_CLASS_MEASUREMENT, # Or total_increasing if it's a peak that only goes up in a period
            # icon="mdi:chart-gantt",
        ),
        cv.Optional("active_energy_import_maximum_demand_last_13_months"): sensor.sensor_schema( # e.g. OBIS 1-0:1.6.0.x
            unit_of_measurement=UNIT_KILOWATT,
            accuracy_decimals=3,
            device_class=DEVICE_CLASS_POWER,
            state_class=STATE_CLASS_MEASUREMENT,
            # icon="mdi:chart-timeline-variant",
        ),
        # Add other standard sensors as needed, mirroring official DSMR component capabilities
        # or common fields from the matthijskooijman/arduino-dsmr parser library.
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_DSMR_CUSTOM_HUB_ID])
    for key, conf_item in config.items():
        if isinstance(conf_item, dict) and key != CONF_DSMR_CUSTOM_HUB_ID:
            var = await sensor.new_sensor(conf_item)
            cg.add(getattr(hub, f"set_{key}")(var))
