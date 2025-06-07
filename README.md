# dsmr_custom - Enhanced DSMR P1 Component for ESPHome

**Version:** (e.g., 1.0.0 - You can version your custom component)
**ESPHome Compatibility:** Tested with ESPHome 2025.5.2 (Update as necessary)
**Original Author of `dsmr_custom`:** (Your Name/Alias here)
**Parser Base:** `glmnet/Dsmr` (version 0.8), vendored and modified. Original by Matthijs Kooijman.

## Introduction

`dsmr_custom` is an advanced custom component for ESPHome designed to read and parse P1 port data from smart meters. It offers significantly enhanced flexibility and compatibility compared to standard solutions, particularly for users with meters adhering to regional standards like the Finnish SESKO (HAN/RJ12 port, ASCII protocol) or those requiring support for non-standard OBIS codes.

This component was developed to address limitations in existing DSMR parsers, such as strict P1 telegram header validation and the inability for users to easily define sensors for any OBIS code present in their meter's output. `dsmr_custom` provides a robust and highly configurable solution for DSMR P1 port integration in ESPHome.

## Key Features

* **User-Defined OBIS Sensors:** Define sensors for *any* OBIS code directly in your ESPHome YAML configuration, including full control over name, unit, device class, state class, icon, and other ESPHome sensor properties. This is ideal for regional OBIS codes or less common meter parameters.
* **Enhanced Vendored Parser (Based on `glmnet/Dsmr` v0.8):**
    * **Lenient P1 Header Parsing:** Modified to correctly parse P1 telegrams with non-standard identification line headers, such as the `/ADN9...` format used by some Finnish meters.
    * **`DEFINE_FIELD` Macro Fix (Internal C++):** Resolved a `reinterpret_cast is not a constant expression` C++ compilation error present in some versions of the underlying parser's field definition macros, ensuring stability and compatibility with modern C++ compilers.
    * **Configurable M-Bus Channel IDs:** Allows M-Bus channel IDs for standard gas, water, etc., meters to be configured via YAML through build flags passed to the parser.
    * **Consistent ESPHome Styling:** Vendored parser files have been styled for consistency with ESPHome C++ conventions (e.g., member naming).
* **Standard DSMR Sensor Support:** Option to define common, predefined DSMR sensors (energy, power, voltage, gas, etc.) via standard `sensor:` and `text_sensor:` platform configurations, similar to native ESPHome components.
* **Sensor Override Mechanism:** If a standard sensor (defined via platform) and a custom OBIS sensor (defined in `custom_obis_sensors`) target the same OBIS code, the custom sensor definition will take precedence, preventing duplicate entities.
* **AES-128 GCM Encrypted Telegram Support:** Decrypts encrypted P1 telegrams (e.g., for Luxembourg meters) using a user-provided 32-character hexadecimal key. Supports dynamic key updates via an API service (see example YAML).
* **`request_pin` Support:** Allows active data requests by controlling the P1 port's Data Request (RTS) pin, for meters that require it.
* **Comprehensive English Comments & ESPHome Styling:** All component code (Python and C++) is thoroughly commented in English, adhering to ESPHome's coding and documentation standards.

## Installation

1.  **Copy Component Files:**
    * Create a directory named `dsmr_custom` inside your ESPHome configuration's `custom_components/` directory. If `custom_components/` doesn't exist, create it in the same directory as your `*.yaml` files.
    * Copy all the following files into the `custom_components/dsmr_custom/` directory:
        * `__init__.py`
        * `sensor.py`
        * `text_sensor.py`
        * `dsmr.h` (hub class header)
        * `dsmr.cpp` (hub class implementation)
        * `crc16.h` (vendored parser file)
        * `fields.h` (vendored parser file)
        * `fields.cpp` (vendored parser file)
        * `parser.h` (vendored parser file)
        * `util.h` (vendored parser file)

2.  **Add to ESPHome YAML:**
    In your device's YAML configuration file (e.g., `slimmelezer.yaml`), add the following to enable the custom component:
    ```yaml
    external_components:
      - source:
          type: local
          path: custom_components/ # Or the relative path to your custom_components folder
        components: [ dsmr_custom ]
    ```

## Configuration

The `dsmr_custom` component is configured in two main parts: the central hub and the sensors.

### 1. Hub Configuration (`dsmr_custom:`)

This block configures the main P1 port interface and parsing behavior.

```yaml
dsmr_custom:
  id: dsmr_hub # Required. This ID is used to link sensors to this hub.
  uart_id: uart_bus # Required. The ID of the UART bus connected to the P1 port.
  max_telegram_length: 1700 # Optional, default: 1500. Max bytes for a telegram.
  receive_timeout: "600ms"  # Optional, default: "200ms". Timeout for receiving data.
  crc_check: true           # Optional, default: true. Perform CRC check on telegrams.
  decryption_key: "YOUR_32_CHAR_HEX_DECRYPTION_KEY" # Optional. For encrypted telegrams.
                                                    # Can also be set dynamically (see example).
  request_pin: D5           # Optional. GPIO pin for Data Request (RTS). E.g., D5.
  request_interval: "10s"   # Optional, default: "0s". Interval for active data requests.
                            # If request_pin is not set, "0s" means continuous read attempts.
  gas_mbus_id: 1            # Optional, default: 1. M-Bus channel ID for standard gas meter.
  water_mbus_id: 2          # Optional, default: 2. M-Bus channel ID for standard water meter.
                            # (Also DSMR_CUSTOM_THERMAL_MBUS_ID and DSMR_CUSTOM_SUB_MBUS_ID can be set via C++ defines if needed)
  # ... plus custom_obis_sensors (see below)
```

* **`id`** (Required, ID): A unique ID for this DSMR hub instance.
* **`uart_id`** (Required, ID): The ID of the `uart` component connected to your P1 port.
* **`max_telegram_length`** (Optional, int, default: `1500`): The maximum expected length of a P1 telegram in bytes. Increase if you experience truncated telegrams.
* **`receive_timeout`** (Optional, Time, default: `200ms`): Timeout for receiving data packets. If no new data arrives within this period, the current telegram attempt might be aborted.
* **`crc_check`** (Optional, boolean, default: `true`): Set to `false` to disable CRC checking, which might be necessary for older DSMR versions or specific meters.
* **`decryption_key`** (Optional, string): Your 32-character hexadecimal AES-128 GCM decryption key if your P1 port sends encrypted telegrams (e.g., Luxembourg). Leave empty or omit to disable decryption. This can also be set dynamically via a service (see `slimmelezer.yaml` example provided to the AI).
* **`request_pin`** (Optional, Pin Schema): The GPIO pin connected to the P1 port's Data Request (RTS) line. If your meter requires an active signal to send data, configure this pin as an output (e.g., `D5`).
* **`request_interval`** (Optional, Time, default: `"0s"`):
    * If `request_pin` is configured: How often to assert the `request_pin` to ask for data.
    * If `request_pin` is NOT configured: How often to attempt a passive read from the UART. A value of `"0s"` implies continuous read attempts.
* **`gas_mbus_id`** (Optional, int, default: `1`): Sets the M-Bus channel ID used by the vendored parser for standard gas meter fields (if defined in `fields.h` to use the `DSMR_CUSTOM_GAS_MBUS_ID` preprocessor define).
* **`water_mbus_id`** (Optional, int, default: `2`): Similar to `gas_mbus_id`, but for standard water meter fields.

### 2. User-Defined OBIS Sensors (`custom_obis_sensors:`)

This is a powerful feature of `dsmr_custom`, allowing you to define sensors for any OBIS code your meter outputs. Add this list under your `dsmr_custom:` hub configuration.

```yaml
dsmr_custom:
  id: dsmr_hub
  # ... other hub settings ...
  custom_obis_sensors:
    - code: "1-0:1.8.0"  # The OBIS code string from your meter's telegram
      name: "My Custom Total Energy Import"
      type: sensor      # "sensor" for numeric, "text_sensor" for text
      # Common sensor parameters (see ESPHome documentation for full list):
      unit_of_measurement: "kWh"
      accuracy_decimals: 3
      device_class: energy
      state_class: total_increasing
      icon: "mdi:home-import-outline"
      # force_update: false
      # disabled_by_default: false
      # entity_category: ""
      # internal: false

    - code: "0-0:96.1.0" # Example: Meter Serial Number
      name: "My Meter Serial"
      type: text_sensor
      icon: "mdi:identifier"

    # Add more custom sensors as needed...
```

**Parameters for each `custom_obis_sensors` entry:**

* **`code`** (Required, string): The exact OBIS code string (e.g., `"1-0:1.8.0"`, `"0-0:96.1.1"`).
* **`name`** (Required, string): The desired name for this sensor in Home Assistant.
* **`type`** (Required, enum): Specifies the type of sensor.
    * `"sensor"`: For numeric values.
    * `"text_sensor"`: For text/string values.
* **For `type: sensor` (Numeric Sensors):**
    * `unit_of_measurement` (Optional, string): E.g., `"kWh"`, `"kW"`, `"V"`, `"A"`, `"m3"`.
    * `accuracy_decimals` (Optional, int): Number of decimal places for the value.
    * `device_class` (Optional, string): Home Assistant device class (e.g., `energy`, `power`, `voltage`, `current`, `gas`, `water`).
    * `state_class` (Optional, string): Home Assistant state class (e.g., `measurement`, `total_increasing`).
    * `icon` (Optional, string): Icon for Home Assistant (e.g., `"mdi:flash"`).
    * `force_update` (Optional, boolean, default: `false`): If `true`, always publish updates even if the value hasn't changed.
* **For `type: text_sensor` (Text Sensors):**
    * `icon` (Optional, string): Icon for Home Assistant.
* **Common for both types:**
    * `disabled_by_default` (Optional, boolean, default: `false`): If `true`, the entity will be disabled by default in Home Assistant and needs to be manually enabled.
    * `entity_category` (Optional, string, default: `""`): Entity category in Home Assistant (e.g., `"config"`, `"diagnostic"`).
    * `internal` (Optional, boolean, default: `false`): If `true`, the sensor is for internal ESPHome use and not exposed to Home Assistant.

### 3. Standard Sensor Platforms

Alternatively, for common DSMR fields known by the vendored parser, you can define them under the global `sensor:` or `text_sensor:` blocks. These will automatically link to the `dsmr_custom` hub.

```yaml
sensor:
  - platform: dsmr_custom
    dsmr_custom_hub_id: dsmr_hub # Link to your hub's ID
    # Enable specific standard sensors by their symbolic name:
    # (These names correspond to fields in the vendored parser's fields.h)
    energy_delivered_tariff1:
      name: "Standard Energy Delivered Tariff 1"
      # You can override default properties like icon, etc. here
    power_delivered:
      name: "Standard Power Delivered"

text_sensor:
  - platform: dsmr_custom
    dsmr_custom_hub_id: dsmr_hub
    identification:
      name: "Standard P1 Meter Identification"
    timestamp:
      name: "Standard P1 Telegram Timestamp"
```
* **`dsmr_custom_hub_id`** (Required, ID): Must match the `id` of your `dsmr_custom:` hub block.
* Symbolic sensor names (e.g., `energy_delivered_tariff1`, `identification`): These correspond to the predefined fields in the vendored parser. Refer to `custom_components/dsmr_custom/sensor.py` and `text_sensor.py` for a list of available standard sensor keys.
* **Override Mechanism:** If you define a sensor in `custom_obis_sensors` that uses the same OBIS code as one of these standard platform sensors, the definition from `custom_obis_sensors` will take precedence.

## Vendored Parser & Modifications

This component includes a **vendored (locally embedded) copy of the DSMR parser library, based on `glmnet/Dsmr` version 0.8** (which itself is derived from `matthijskooijman/arduino-dsmr`). Vendoring was chosen to:
1.  Apply critical modifications not yet available in commonly used upstream libraries, ensuring wider meter compatibility.
2.  Resolve potential PlatformIO library fetching issues encountered during development.
3.  Provide a stable parser base for the component's advanced features.

The source files for the vendored parser (`parser.h`, `fields.h`, `fields.cpp`, `util.h`, `crc16.h`) are located directly within the `custom_components/dsmr_custom/` directory and are compiled as part of the component.

**Key Modifications to the Vendored Parser:**

1.  **Lenient P1 Header Parsing (in `parser.h`):**
    * **Problem:** Standard DSMR parsers often strictly validate the P1 identification line (e.g., `/XXX5...` or `/XXX3...`). This causes incompatibility with meters in some regions, like Finland (SESKO standard), which use different formats (e.g., `/ADN9...`).
    * **Solution:** The `P1Parser::parse_data` method has been modified to check if the character following the 3-letter manufacturer ID is simply alphanumeric, rather than requiring a specific '3' or '5'. This significantly improves out-of-the-box compatibility.
2.  **`DEFINE_FIELD` Macro Fix (in `fields.h`):**
    * **Problem:** The original `DEFINE_FIELD` macro (used to declare individual OBIS data fields) often leads to a `reinterpret_cast is not a constant expression` C++ compilation error with modern compilers when defining field names stored in PROGMEM.
    * **Solution:** The macro was modified to change the `static constexpr char name[]` member to a `static constexpr char name_progmem_[]` array for the string literal and a `static const __FlashStringHelper* name()` method that performs the `reinterpret_cast`. This resolves the compilation error.
    * Additionally, members generated by `DEFINE_FIELD` (value, presence flag, OBIS ID) now consistently use trailing underscores (e.g., `fieldname_`, `fieldname_present_`, `id_`) for alignment with ESPHome C++ styling.
3.  **Configurable M-Bus Channel IDs (via `fields.h` and `__init__.py`):**
    * The vendored `fields.h` has been adapted to use preprocessor defines like `DSMR_CUSTOM_GAS_MBUS_ID` and `DSMR_CUSTOM_WATER_MBUS_ID`.
    * These defines are set by `dsmr_custom/__init__.py` using build flags derived from the `gas_mbus_id` and `water_mbus_id` YAML configuration options. This allows users to specify the M-Bus channel for standard gas/water meter readings if their meter uses a non-default channel.
4.  **Consistent Naming in `ParseResult` / `ObisId` (in `util.h`):**
    * Members of the `ParseResult` struct (e.g., `result_`, `next_`, `err_`, `ctx_`) and `ObisId` struct (`v_`) have been standardized with trailing underscores. All parser files (`parser.h`, `fields.h`) have been updated to use these consistent names. This resolved internal compilation errors.
5.  **Unique Include Guards & ESPHome Styling:**
    * All vendored parser header files use unique include guards prefixed with `DSMR_CUSTOM_` to prevent conflicts.
    * Minor C++ styling adjustments have been made for consistency. All comments have been translated to/ensured to be in English.

## Notes for ESPHome Developers (Potential Upstreaming Considerations)

The `dsmr_custom` component showcases several features and solutions that could be beneficial for the native ESPHome DSMR component:

1.  **User-Defined OBIS Sensors:** The `custom_obis_sensors` mechanism is the most significant enhancement. Integrating a similar YAML-based system for defining arbitrary OBIS sensors into the native component would greatly improve its flexibility and adaptability for various regional standards and meter models, reducing the need for users to write custom C++ code or for frequent updates to the core parser library to add new OBIS codes.
2.  **Lenient P1 Header Parsing:** The modified logic in `parser.h` to tolerate variations in the P1 identification line (e.g., alphanumeric character after manufacturer ID) significantly improves out-of-the-box compatibility with a wider range of smart meters, such as Finnish ones. Adopting a similar, more tolerant parsing strategy in the native component's parser library (`glmnet/Dsmr`) is recommended.
3.  **Dynamic Decryption Key Management:** The pattern for managing the decryption key dynamically via an API service and `globals` (as demonstrated in the user's `slimmelezer.yaml` example) offers better usability than relying solely on a static key in YAML, especially if keys need to be provisioned or updated without a full firmware reflash.
4.  **Exposing Raw P1 Identification Line:** The `dsmr_custom` component, via its platform `text_sensor:`, allows easy exposure of the raw P1 identification header. This is invaluable for debugging and community support. The native component could benefit from a similar built-in text sensor.
5.  **Parser Robustness (Compilation):** The `DEFINE_FIELD` macro fix addresses a C++ compilation issue that can occur with some toolchains when using PROGMEM strings with `constexpr`. This fix, if applicable to the parser library used by native ESPHome, would improve build reliability.

## License

This custom component includes a vendored parser based on `glmnet/Dsmr` (originally `matthijskooijman/arduino-dsmr`), which is licensed under the MIT License. The modifications and the surrounding `dsmr_custom` component code are also provided under the MIT License. (You can adjust this as you see fit).

```
MIT License

Copyright (c) [Year] [Your Name/Alias]

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
