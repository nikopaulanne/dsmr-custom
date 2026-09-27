# dsmr-custom - Enhanced DSMR P1 Component for ESPHome

**Latest release:** v1.2.0

**ESPHome Compatibility (current source):** ESPHome 2025.5.0 is the minimum build-tested release; CI also checks the current release.

**Home Assistant validation (current source):** Hardware smoke-tested with Home Assistant Core 2026.9.3 and ESPHome Device Builder 2026.9.0. Earlier Home Assistant releases are outside the current support target.

## Framework Support

This component supports both Arduino and ESP-IDF frameworks with different feature sets:

### Supported Frameworks

- ✅ **Arduino Framework**: ESP8266 and ESP32 (both compile-tested). Live unencrypted P1 data was verified on a Slimmelezer running Arduino.
  - Encrypted telegram support remains experimental. It was previously reported working on a D1 Mini with v1.2.0, but has not been validated in the current hardware test.
- ⚠️ **ESP-IDF Framework**: ESP32 and ESP32-C6 (compile-tested).
  - Encrypted telegram support remains experimental; there is no encrypted-meter hardware test. AES-GCM host tests pass, the ESP-IDF 5 ESP32/ESP32-C6 builds compile, and native and PlatformIO ESP-IDF 6.0.1 ESP32 builds compile and link. The PlatformIO build needs its generated bootloader linker script preprocessed once on a clean build.

### Which Framework Should I Use?

| **Your Situation** | **Recommended Framework** |
|-------------------|--------------------------|
| My meter sends **encrypted** telegrams | ⚠️ Arduino (reported on D1 Mini) or ESP-IDF (compile-tested; field testing needed) |
| My meter sends **unencrypted** telegrams | ✅ Arduino or ESP-IDF |
| I have an ESP32-C6 | ✅ ESP-IDF (compile-tested) |
| I have ESP8266 or classic ESP32 | ✅ Arduino (both compile-tested) |

> **Note:** Most Dutch smart meters send unencrypted telegrams. Check your meter's specifications if unsure.

### Configuration Examples

**Arduino (ESP8266 - Recommended)**
```yaml
esp8266:
  board: d1_mini

dsmr_custom:
  decryption_key: !secret dsmr_decryption_key  # Experimental; store the key in secrets.yaml
```

**ESP-IDF (ESP32-C6)**
```yaml
esp32:
  board: esp32-c6-devkitc-1
  framework:
    type: esp-idf

dsmr_custom:
  decryption_key: !secret dsmr_decryption_key  # Experimental; store the key in secrets.yaml
```

**Parser Base:** `glmnet/Dsmr` (v0.8), vendored and modified. Original by Matthijs Kooijman.

`dsmr-custom` is an advanced custom component for ESPHome designed to read and parse data from the P1 port of smart meters. It offers significant improvements in flexibility and compatibility, especially for meters that do not fully adhere to standards (such as meters following the SESKO standard common in Finland).

---

## Quick Start: Find Your Meter's OBIS Codes via Logs

This guide will get you running in minutes and show you the most stable way to discover all the data your specific meter provides by reading the device logs directly in Home Assistant.

### Step 1: Add the Component to your Configuration

Instead of copying files manually, you can add this component directly to your device's `.yaml` file. The `main` branch contains the current compatibility work; once a new release is published, pin its version tag for reproducible installs.

Add the following `external_components` block to your YAML:
```yaml
external_components:
  - source:
      type: git
      url: https://github.com/nikopaulanne/dsmr-custom
      ref: main
    components: [ dsmr_custom ]
```

### Step 2: Initial Test & Logging Configuration

Use this minimal configuration first. Its only purpose is to safely view the raw data from your meter in the ESPHome logs without crashing the device.

```yaml
# In your secrets.yaml file, you should have:
# wifi_ssid: "YourNetwork"
# wifi_password: "YourPassword"
# esphome_api_encryption_key: "GENERATE_A_KEY_HERE"
# dsmr_decryption_key: "YOUR_32_HEX_CHARACTER_KEY" # Only for encrypted meters

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
  - platform: esphome

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
      ref: main
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
      name: "Full Telegram"
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
2.  Open **ESPHome Device Builder** in Home Assistant.
3.  Select `dsmr-diagnostics` and open its logs.
4.  Wait for the device to connect and receive a data packet from your meter.
5.  You will see a clearly marked block of text appear in the logs, starting with `--- FULL TELEGRAM RECEIVED ---`. This is the complete, raw data packet from your meter.
6.  Copy the block locally to identify the OBIS codes your meter provides. Treat raw telegrams as private; remove meter identifiers and live readings before sharing them publicly.

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
* **Encrypted Telegram Support (Experimental):** Supports AES-128 GCM encrypted P1 telegrams (e.g., for Luxembourg meters). Arduino decryption was reported working on a D1 Mini with v1.2.0; ESP-IDF has compile verification but needs field reports from users with encrypted meters. Other meter and board combinations remain unverified.
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
  decryption_key: !secret dsmr_decryption_key # Optional; keep the key in secrets.yaml.
  # Note: See docs/aes-gcm-implementation-notes.md for technical details on ESP-IDF encryption support
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

