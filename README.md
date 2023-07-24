# Prometheus ESP8266 DHT Exporter

An IoT Prometheus exporter for measuring temperature and humidity, using an ESP8266 (Arduino-compatible) with a Wi-Fi module and a DHT (temperature + humidity) sensor.

## Metrics

| Metric | Description | Unit |
| - | - | - |
| `dorm_info` | Metadata about the device. | |
| `dorm_free_heap` | Free heap. | `byte` |
| `dorm_sensor` | Metadata about a sensor. | |
| `dorm_air_humidity_percent` | Air humidity. | `%H` |
| `dorm_air_temperature_celsius` | Air temperature. | `°C` |
| `dorm_air_heat_index_celsius` | Apparent air temperature, based on temperature and humidity. | `°C` |
| `dorm_air_quality_eco2` | Equivalent calculated carbon-dioxide. | `ppm` |
| `dorm_air_quality_tvoc` | Total volatile organic compound. | `ppb/t` |
| `dorm_air_quality_h2` | Hydrogen measurement. | `ppm` |
| `dorm_air_quality_ethanol` | Ethanol measurement. | `ppm` |

## Requirements

### Hardware

- ESP8266-based board (or some other appropriate Arduino-based board).
    - Tested with "Adafruit Feather HUZZAH ESP8266" and "WEMOS D1 Mini".
- DHT sensor.
    - Tested with a cheap DHT11 from eBay (using pin 12).
    - DHT11 supports a maximum of 1Hz polling while DHT22 supports a maximum of 2Hz polling.
    - Both DHT11 and DHT22 support both 3V and 5V at 2.5mA max current.
- Adafruit SGP30 Sensor

### Software

- [PlatformIO](https://platformio.org/)

## Building

### Hardware

Using the "Adafruit Feather HUZZAH ESP8266".

Wire the DHT sensor power to the 3.3V and any GND on the ESP and wire the data output to e.g. pin 14 (aka D5).

## Version

See `src/version.h`.

It's set manually since no build tools (or CI) other than the Arduino IDE is used.

## License

GNU General Public License version 3 (GPLv3).