# dsmr-custom - Enhanced DSMR P1 Component for ESPHome

**Version:** 1.0.2

**ESPHome Compatibility:** 2025.5.x or newer

## Framework Support

This component supports both Arduino and ESP-IDF frameworks with different feature sets:

### Supported Frameworks

- ✅ **Arduino Framework**: ESP8266, ESP32, ESP32-S2/S3/C3
  - **Full support** including encrypted telegram decryption
  - Tested and stable
- ⚠️ **ESP-IDF Framework**: ESP32, ESP32-C6, ESP32-H2
  - Supports **unencrypted telegrams only**
  - Encrypted telegram decryption **NOT supported** (mbedtls linkage limitation)
  - Use for newer chips (C6/H2) or when ESP-IDF is required

### Which Framework Should I Use?

| **Your Situation** | **Recommended Framework** |
|-------------------|--------------------------|
| My meter sends **encrypted** telegrams | ✅ Arduino |
| My meter sends **unencrypted** telegrams | ✅ Arduino or ESP-IDF |
| I have ESP32-C6 or ESP32-H2 | ⚠️ ESP-IDF (unencrypted only) |
| I have ESP8266 or classic ESP32 | ✅ Arduino (fully tested) |

> **Note:** Most Dutch smart meters send unencrypted telegrams. Check your meter's specifications if unsure.

### Configuration Examples

**Arduino (ESP8266 - Recommended)**
```yaml
esp8266:
  board: d1_mini

dsmr_custom:
  decryption_key: "AABBCCDDEEFF00112233445566778899"  # ✅ Supported
```

**ESP-IDF (ESP32-C6 - Unencrypted Only)**
```yaml
esp32:
  board: esp32-c6-devkitc-1
  framework:
    type: esp-idf

dsmr_custom:
  # ⚠️ Do NOT set decryption_key - not supported
  # Encrypted telegrams will be skipped with warning in logs
```

### ESP-IDF Limitations

**Encrypted telegram decryption is NOT supported in ESP-IDF builds.**

- **Why?** mbedtls library linkage limitations in ESPHome's ESP-IDF build system
- **Impact:** Encrypted telegrams are skipped with clear warning messages in logs
- **Workaround:** Use Arduino framework for encrypted telegrams
- **Tracking:** [See detailed explanation](docs/esp-idf-limitations.md)

**Author:** Niko Paulanne

**Parser Base:** `glmnet/Dsmr` (v0.8), vendored and modified. Original by Matthijs Kooijman.

`dsmr-custom` is an advanced custom component for ESPHome designed to read and parse data from the P1 port of smart meters. It offers significant improvements in flexibility and compatibility, especially for meters that do not fully adhere to standards (such as meters following the SESKO standard common in Finland).

---

## Quick Start: Find Your Meter's OBIS Codes via Logs

This guide will get you running in minutes and show you the most stable way to discover all the data your specific meter provides by reading the device logs directly in Home Assistant.

### Step 1: Add the Component to your Configuration

Instead of copying files manually, you can add this component directly to your device's `.yaml` file. This will make ESPHome automatically download the correct version from GitHub.

Add the following `external_components` block to your YAML:
```yaml
external_components:
  - source:
      type: git
      url: https://github.com/nikopaulanne/dsmr-custom
      ref: v1.0.2
    components: [ dsmr_custom ]
```

### Step 2: Initial Test & Logging Configuration

Use this minimal configuration first. Its only purpose is to safely view the raw data from your meter in the ESPHome logs without crashing the device.

```yaml
# In your secrets.yaml file, you should have:
# wifi_ssid: "YourNetwork"
# wifi_password: "YourPassword"
# esphome_api_encryption_key: "GENERATE_A_KEY_HERE"

esphome:
  name: dsmr-diagnostics

esp8266:
  board: d1_mini # Change to match your board

wifi:
  ssid: !secret wifi_ssid
  password: !secret wifi_password

api:
  encryption:
    key: !secret esphome_api_encryption_key

ota:

# The logger is essential for viewing the telegram data
logger:
  level: INFO # INFO level is sufficient for this purpose

uart:
  id: uart_bus
  baud_rate: 115200
  rx_pin: D7 # Check the pinout for your hardware
  rx_buffer_size: 1700

external_components:
  - source:
      type: git
      url: https://github.com/nikopaulanne/dsmr-custom
      ref: 1.0.2
    components: [ dsmr_custom ]

# --- DSMR Hub ---
dsmr_custom:
  id: dsmr_hub
  uart_id: uart_bus

# --- Sensors for Logging & Diagnostics Only ---
text_sensor:
  - platform: dsmr_custom
    dsmr_custom_hub_id: dsmr_hub
    
    # This sensor helps confirm a connection to the meter
    identification:
      name: "P1 Telegram Header"
      
    # This captures the full telegram but does NOT send it to a Home Assistant state.
    # Instead, a lightweight on_value trigger prints the data to the logs.
    telegram:
      internal: true # This prevents the state from being sent to Home Assistant
      on_value:
        - logger.log:
            level: INFO
            tag: "telegram_dump" # Makes the message easy to find in logs
            format: "--- FULL TELEGRAM RECEIVED ---\n%s\n-----------------------------"
            args: [x.c_str()]
```

### Step 3: View Logs and Collect OBIS Codes

1.  Install the minimal configuration above to your device.
2.  In Home Assistant, navigate to **Settings > Add-ons > ESPHome**.
3.  Select your device (`dsmr-diagnostics`) from the dashboard and click the **LOGS** button.
4.  Wait for the device to connect and receive a data packet from your meter.
5.  You will see a clearly marked block of text appear in the logs, starting with `--- FULL TELEGRAM RECEIVED ---`. This is the complete, raw data packet from your meter.
6.  **Copy this entire block of text** (from the start line to the end line) to your clipboard. You now have a complete list of all OBIS codes your meter provides.

### Step 4: Configure Your Final Sensors

Now that you have your list of OBIS codes, you can create your final, permanent configuration. Modify your YAML file, remove the diagnostic `on_value` trigger, and add the sensors you want using the `custom_obis_sensors` list.

See the `slimmelezer-example.yaml` file for a detailed example.

```yaml
dsmr_custom:
  id: dsmr_hub
  uart_id: uart_bus
  # ... other hub settings ...
  
  custom_obis_sensors:
    # Add sensors here using the codes you found in the logs
    - code: "1-0:1.8.0" 
      name: "Total Energy Import"
      type: sensor
      # ...
```

---

## Full Documentation

### Key Features

* **User-Defined OBIS Sensors:** Define sensors for *any* OBIS code directly in your ESPHome YAML configuration, including full control over name, unit, device class, state class, icon, and other ESPHome sensor properties.
* **Enhanced Vendored Parser:** Based on `glmnet/Dsmr`, this component includes critical fixes:
    * **Lenient P1 Header Parsing:** Correctly parses P1 telegrams with non-standard identification lines (e.g., `/ADN9...` used in Finland).
    * **`DEFINE_FIELD` Macro Fix:** Resolves a C++ compilation error found in some versions of the underlying parser.
    * **Configurable M-Bus Channel IDs:** Allows M-Bus channel IDs for gas/water meters to be configured via YAML.
* **Standard DSMR Sensor Support:** Option to define common sensors (energy, power, etc.) via standard `sensor:` and `text_sensor:` platforms.
* **Sensor Override Mechanism:** A custom OBIS sensor will always take precedence over a standard sensor if they target the same OBIS code, preventing duplicate entities.
* **Encrypted Telegram Support (Experimental):** Decrypts AES-128 GCM encrypted P1 telegrams (e.g., for Luxembourg meters). **Note: This feature is considered experimental and has not been tested on physical hardware.**
* **`request_pin` Support:** Allows active data requests by controlling the P1 port's Data Request (RTS) pin.

### Detailed Configuration

#### Hub Configuration (`dsmr_custom:`)

This block configures the main P1 port interface. All parameters from the example below can be added to your `dsmr_custom:` block.

```yaml
dsmr_custom:
  id: dsmr_hub # Required. This ID is used to link sensors to this hub.
  uart_id: uart_bus # Required. The ID of the UART bus connected to the P1 port.
  max_telegram_length: 1700 # Optional, default: 1500. Max bytes for a telegram.
  receive_timeout: "600ms"  # Optional, default: "200ms". Timeout for receiving data.
  crc_check: true           # Optional, default: true. Perform CRC check on telegrams.
  decryption_key: "YOUR_32_CHAR_HEX_DECRYPTION_KEY" # Optional. For encrypted telegrams. (Experimental, not tested on real hardware)
  request_pin: D5           # Optional. GPIO pin for Data Request (RTS). E.g., D5.
  request_interval: "10s"   # Optional, default: "0s". Interval for active data requests.
  gas_mbus_id: 1            # Optional, default: 1. M-Bus channel ID for standard gas meter.
  water_mbus_id: 2          # Optional, default: 2. M-Bus channel ID for standard water meter.
```

#### User-Defined OBIS Sensors (`custom_obis_sensors:`)

This is the most powerful feature. Add this list under your `dsmr_custom:` hub configuration.

```yaml
dsmr_custom:
  id: dsmr_hub
  # ... other hub settings ...
  custom_obis_sensors:
    - code: "1-0:1.8.0"  # The OBIS code string from your meter's telegram
      name: "My Custom Total Energy Import"
      type: sensor      # "sensor" for numeric, "text_sensor" for text
      # All standard ESPHome sensor parameters are supported:
      unit_of_measurement: "kWh"
      accuracy_decimals: 3
      device_class: energy
      state_class: total_increasing
      icon: "mdi:home-import-outline"
```

### Vendored Parser & Modifications

This component includes a **vendored (locally embedded) copy of the `glmnet/Dsmr` parser library** to apply critical fixes not available upstream. This ensures wider meter compatibility and build stability.

* **Lenient P1 Header Parsing:** The parser was modified to accept non-standard identification lines, like those from Finnish meters.
* **`DEFINE_FIELD` Macro Fix:** A C++ compilation error related to PROGMEM was resolved, making the component compatible with modern compilers.
* **Consistent Naming:** Parser-internal struct members were standardized to fix compilation errors.

### Notes for ESPHome Developers (Potential Upstreaming)

This component demonstrates features that could benefit the native ESPHome DSMR component:
1.  **User-Defined OBIS Sensors:** A native YAML-based system for defining arbitrary OBIS sensors would greatly improve flexibility.
2.  **Lenient P1 Header Parsing:** A more tolerant parsing strategy would improve out-of-the-box compatibility with a wider range of meters.
3.  **Exposing Raw P1 Identification/Telegram:** Built-in text sensors for the P1 header and the full telegram are invaluable for debugging.

## License

This component is licensed under the **GNU General Public License v3.0**. A copy of the license is included in the `LICENSE` file in this repository.

This project incorporates and modifies code from the `glmnet/Dsmr` library (which is based on `matthijskooijman/arduino-dsmr`). That original work is licensed under the MIT License. In compliance with the licensing terms, the original copyright notices and permissions are preserved in the headers of the respective source files (`parser.h`, `fields.h`, `util.h`, etc.).

