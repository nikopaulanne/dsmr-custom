# dsmr-custom - Enhanced DSMR P1 Component for ESPHome

**Version:** 1.0.0

**ESPHome Compatibility:** 2025.5.x or newer

**Author:** Niko Paulanne

**Parser Base:** `glmnet/Dsmr` (v0.8), vendored and modified. Original by Matthijs Kooijman.

`dsmr-custom` is an advanced custom component for ESPHome designed to read and parse data from the P1 port of smart meters. It offers significant improvements in flexibility and compatibility, especially for meters that do not fully adhere to standards (such as meters following the SESKO standard common in Finland).

---

## Quick Start: Find Your Meter's OBIS Codes

This guide will get you running in minutes and show you how to discover all the data your specific meter provides.

### Step 1: Installation

1.  **Copy Component Files:** Create a `custom_components/dsmr_custom/` directory in your ESPHome config folder and copy all the files from this repository into it.
2.  **Add to YAML:** Add the following to your device's `.yaml` file:
    ```yaml
    external_components:
      - source:
          type: local
          path: custom_components/
        components: [ dsmr_custom ]
    ```

### Step 2: Initial Test Configuration

Use this minimal configuration first. Its only purpose is to view the raw data from your meter in Home Assistant.

```yaml
# In your secrets.yaml file:
# wifi_ssid: "YourNetwork"
# wifi_password: "YourPassword"
# esphome_api_encryption_key: "GENERATE_A_KEY_HERE"

esphome:
  name: dsmr-reader

esp8266:
  board: d1_mini # Change to match your board

wifi:
  ssid: !secret wifi_ssid
  password: !secret wifi_password

api:
  encryption:
    key: !secret esphome_api_encryption_key

ota:
logger:

uart:
  id: uart_bus
  baud_rate: 115200
  rx_pin: D7 # Check the pinout for your hardware
  rx_buffer_size: 1700

external_components:
  - source:
      type: local
      path: custom_components/
    components: [ dsmr_custom ]

# --- DSMR Hub ---
dsmr_custom:
  id: dsmr_hub
  uart_id: uart_bus

# --- Text Sensors for Troubleshooting ---
text_sensor:
  - platform: dsmr_custom
    dsmr_custom_hub_id: dsmr_hub
    
    # 1. Identify your meter: This sensor shows the first line of the P1 telegram.
    identification:
      name: "P1 Telegram Header"
      
    # 2. See all data: This sensor shows the ENTIRE raw P1 telegram.
    telegram:
      name: "DSMR Full Telegram"
      internal: false # IMPORTANT: This makes the sensor visible in Home Assistant
```

### Step 3: View Data and Collect OBIS Codes

1.  Upload the minimal configuration above to your device.
2.  In Home Assistant, find the two new sensors: **"P1 Telegram Header"** and **"DSMR Full Telegram"**.
3.  The "P1 Telegram Header" confirms your meter model.
4.  Open the state or history of the **"DSMR Full Telegram"** sensor. You will see the entire P1 telegram as raw text.
5.  Copy this text and inspect it. You can now see a list of all the OBIS codes your meter is sending.

### Step 4: Configure Your Sensors

Now that you know your OBIS codes, expand your YAML to include the `custom_obis_sensors` list. Use the codes you found to create sensors. See the `slimmelezer-example.yaml` file for a detailed example.

```yaml
dsmr_custom:
  id: dsmr_hub
  uart_id: uart_bus
  
  custom_obis_sensors:
    - code: "1-0:1.8.0" # OBIS code from your telegram
      name: "Total Energy Import"
      type: sensor
      unit_of_measurement: "kWh"
      device_class: energy
      state_class: total_increasing
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
* **Encrypted Telegram Support:** Decrypts AES-128 GCM encrypted P1 telegrams (e.g., for Luxembourg meters).
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
  decryption_key: "YOUR_32_CHAR_HEX_DECRYPTION_KEY" # Optional. For encrypted telegrams.
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
```
