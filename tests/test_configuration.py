"""Validate configuration and generated field lists without toolchain downloads."""
import os
from pathlib import Path
import runpy
import subprocess
import sys
import tempfile
import types
import unittest
from unittest.mock import patch

REPO = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(REPO / "components"))
import dsmr_custom
from esphome import config_validation as cv
from esphome.core import CORE


class ConfigurationTests(unittest.TestCase):
    def setUp(self):
        CORE.reset()

    def test_keys_are_exact_hex(self):
        self.assertEqual(dsmr_custom._validate_key("00112233445566778899aabbccddeeff"),
                         "00112233445566778899AABBCCDDEEFF")
        self.assertEqual(dsmr_custom._validate_key(""), "")
        for value in ("+1" * 16, "-1" * 16, " F" * 16, "\t1" * 16,
                      "G1" * 16, "01" * 15, "01" * 17, "０1" * 16):
            with self.subTest(value=value), self.assertRaises(cv.Invalid):
                dsmr_custom._validate_key(value)

    def test_native_sensor_properties(self):
        result = dsmr_custom.CUSTOM_OBIS_SENSOR_SCHEMA({
            "code": "1-0:1.8.0*255", "type": "sensor", "name": "Synthetic energy",
            "id": "custom_energy", "device_class": "energy", "accuracy_decimals": 3,
            "filters": [{"multiply": 2}], "on_value": [{"then": [{"logger.log": "updated"}]}],
        })
        self.assertEqual(str(result["id"]), "custom_energy")
        self.assertIn("filters", result)
        self.assertIn("on_value", result)
        with self.assertRaises(cv.Invalid):
            dsmr_custom.CUSTOM_OBIS_SENSOR_SCHEMA({
                "code": "1-0:1.8.0", "type": "sensor", "name": "Synthetic",
                "device_class": "not_a_device_class",
            })

    def test_text_sensor_properties(self):
        result = dsmr_custom.CUSTOM_OBIS_SENSOR_SCHEMA({
            "code": "1-0:1.8.0", "type": "text_sensor", "name": "Synthetic raw",
            "id": "custom_raw", "filters": [{"to_upper": None}],
        })
        self.assertEqual(str(result["id"]), "custom_raw")
        self.assertIn("filters", result)
        with self.assertRaises(cv.Invalid):
            dsmr_custom.CUSTOM_OBIS_SENSOR_SCHEMA({
                "code": "1-0:1.8.0", "type": "text_sensor", "name": "Synthetic",
                "accuracy_decimals": 3,
            })

    def test_buffer_bounds(self):
        for length in (0, 65536, 2 ** 32):
            with self.assertRaises(cv.Invalid):
                dsmr_custom.CONFIG_SCHEMA({"uart_id": "synthetic_uart", "max_telegram_length": length})

    def test_linker_archive_layouts(self):
        script = types.ModuleType("SCons.Script")
        script.Import = lambda name: None
        env = types.SimpleNamespace(AddPreAction=lambda *args: None)
        with patch.dict(sys.modules, {"SCons": types.ModuleType("SCons"), "SCons.Script": script}):
            find_archive = runpy.run_path(str(REPO / "components/dsmr_custom/post_build.py"),
                                         init_globals={"env": env})["find_static_library"]
        with tempfile.TemporaryDirectory(prefix="dsmr-linker-tests-") as workdir:
            root = Path(workdir)
            upstream = root / "mbedtls/library"
            upstream.mkdir(parents=True)
            (root / "libmbedtls.a").touch()
            (upstream / "libmbedtls.a").touch()
            self.assertEqual(find_archive(str(root), "mbedtls"), str(upstream))
            for library in ("mbedx509", "mbedcrypto", "tfpsacrypto"):
                (upstream / f"lib{library}.a").touch()
                self.assertEqual(find_archive(str(root), library), str(upstream))
            other = root / "unexpected"
            other.mkdir()
            (other / "libmbedtls.a").touch()
            with self.assertRaises(RuntimeError):
                find_archive(str(root), "mbedtls")

    def test_generated_lists_compile(self):
        # Include standard-only, custom-only, empty text list, repeated blocks,
        # and filters/actions through actual ESPHome source generation.
        cases = {
            "custom-only": """dsmr_custom:
  uart_id: synthetic_uart
  custom_obis_sensors:
    - code: '1-0:1.8.0*255'
      type: sensor
      name: Synthetic energy
      id: custom_energy
      filters:
        - multiply: 2
      on_value:
        then:
          - logger.log: updated
    - code: '1-0:1.8.0'
      type: text_sensor
      name: Synthetic raw
      id: custom_raw
""",
            "numeric-and-telegram": """dsmr_custom:
  id: hub
  uart_id: synthetic_uart
sensor:
  - platform: dsmr_custom
    dsmr_custom_hub_id: hub
    energy_delivered_lux:
      name: Synthetic energy
text_sensor:
  - platform: dsmr_custom
    dsmr_custom_hub_id: hub
    telegram:
      name: Synthetic telegram
""",
            "multiple-blocks": """dsmr_custom:
  id: hub
  uart_id: synthetic_uart
sensor:
  - platform: dsmr_custom
    dsmr_custom_hub_id: hub
    energy_delivered_lux:
      name: Synthetic energy
  - platform: dsmr_custom
    dsmr_custom_hub_id: hub
    power_delivered:
      name: Synthetic power
text_sensor:
  - platform: dsmr_custom
    dsmr_custom_hub_id: hub
    identification:
      name: Synthetic header
  - platform: dsmr_custom
    dsmr_custom_hub_id: hub
    timestamp:
      name: Synthetic time
""",
            "telegram-only": """dsmr_custom:
  id: hub
  uart_id: synthetic_uart
text_sensor:
  - platform: dsmr_custom
    dsmr_custom_hub_id: hub
    telegram:
      name: Synthetic telegram
""",
        }
        with tempfile.TemporaryDirectory(prefix="dsmr-config-tests-") as workdir:
            root = Path(workdir)
            for name, fragment in cases.items():
                with self.subTest(name=name):
                    config = root / f"{name}.yaml"
                    config.write_text(f"""esphome:
  name: {name}
esp8266:
  board: d1_mini
logger:
uart:
  id: synthetic_uart
  baud_rate: 115200
  rx_pin: D7
external_components:
  - source:
      type: local
      path: {REPO / 'components'}
    components: [dsmr_custom]
""" + fragment)
                    env = dict(os.environ, ESPHOME_DATA_DIR=str(root / name), ESPHOME_BUILD_PATH="build")
                    generated = subprocess.run([sys.executable, "-m", "esphome", "compile", "--only-generate", str(config)],
                                               env=env, capture_output=True, text=True)
                    self.assertEqual(generated.returncode, 0, generated.stdout + generated.stderr)
                    build = root / name / "build" / name
                    defines = (build / "src/esphome/core/defines.h").read_text()
                    lists = [line for line in defines.splitlines() if line.startswith("#define DSMR_CUSTOM_")]
                    self.assertEqual(sum(line.startswith("#define DSMR_CUSTOM_SENSOR_LIST(") for line in lists), 1)
                    self.assertEqual(sum(line.startswith("#define DSMR_CUSTOM_TEXT_SENSOR_LIST(") for line in lists), 1)
                    source = root / f"{name}.cpp"
                    source.write_text("#define USE_ESP_IDF\n" + "\n".join(lists) + '\n#include "dsmr.h"\n')
                    syntax = subprocess.run([os.environ.get("CXX", "c++"), "-std=c++17", "-fsyntax-only",
                                             "-I", str(REPO / "tests/host/runtime"), "-I", str(REPO / "components/dsmr_custom"),
                                             str(source)], capture_output=True, text=True)
                    self.assertEqual(syntax.returncode, 0, syntax.stderr)
                    main = (build / "src/main.cpp").read_text()
                    if name == "custom-only":
                        self.assertIn("custom_energy", main)
                        self.assertIn("MultiplyFilter", main)
                        self.assertIn('1-0:1.8.0*255', main)
                    elif name == "multiple-blocks":
                        self.assertIn("set_energy_delivered_lux", main)
                        self.assertIn("set_power_delivered", main)


if __name__ == "__main__":
    unittest.main()
