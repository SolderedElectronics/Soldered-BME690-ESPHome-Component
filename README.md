# Soldered BME690 ESPHome Component

| ![BME690 breakout board](https://raw.githubusercontent.com/SolderedElectronics/Soldered-BME690-ESPHome-Component/main/extras/BME690.jpg) |
| :--------------------------------------------------------------------------------------------------------------------------------------: |
|                                  [BME690 breakout board](https://www.solde.red/333411)                                  |

Breakout board for the Bosch BME690 sensor, which measures temperature, relative humidity, barometric pressure and gas
resistance (VOC). The board communicates over I2C only and is part of the
[Qwiic ecosystem](https://soldered.com/collections/qwiic-ecosystem).

External ESPHome component for the Soldered BME690 breakout board. It is a port of the
[Soldered BME690 Arduino library](https://github.com/SolderedElectronics/Soldered-BME690-Arduino-Library) and wraps the
official Bosch BME69x Sensor API, which is vendored unmodified in `components/bme690/bme69x.c`, `bme69x.h` and
`bme69x_defs.h`.

## Repository Contents

- **components/** - the ESPHome external component (Python config + C++ implementation)
- **examples/** - example YAML configs showing how to use the component

## Usage

Reference this repo directly from your own ESPHome YAML (no need to clone it locally):

```yaml
external_components:
  - source: github://SolderedElectronics/Soldered-BME690-ESPHome-Component
    components: [bme690]

i2c:
  sda: GPIO21
  scl: GPIO22

sensor:
  - platform: bme690
    address: 0x76
    update_interval: 60s
    temperature:
      name: "BME690 Temperature"
    pressure:
      name: "BME690 Pressure"
    humidity:
      name: "BME690 Humidity"
    gas_resistance:
      name: "BME690 Gas Resistance"
```

See [`examples/forced_mode.yaml`](examples/forced_mode.yaml) for a full working example.

### Configuration variables

- **address** (*Optional*, int): I2C address of the sensor. Defaults to `0x76`, use `0x77` if the address jumper is
  soldered.
- **update_interval** (*Optional*, [Time](https://esphome.io/guides/configuration-types#config-time)): how often the
  values are published. Defaults to `60s`.
- **operation_mode** (*Optional*, string): `forced`, `parallel` or `sequential`. Defaults to `forced`.
- **iir_filter** (*Optional*, string): coefficient of the IIR filter applied to the temperature and pressure readings.
  One of `OFF`, `1X`, `3X`, `7X`, `15X`, `31X`, `63X`, `127X`. Defaults to `OFF`.
- **odr** (*Optional*, string): sleep duration between two profile steps in sequential mode. One of `0.59ms`, `10ms`,
  `20ms`, `62.5ms`, `125ms`, `250ms`, `500ms`, `1000ms`, `none`. Defaults to `0.59ms`.
- **temperature**, **pressure**, **humidity** (*Optional*): [Sensors](https://esphome.io/components/sensor/) published
  in °C, hPa and %. Each one also takes an **oversampling** option, one of `NONE`, `1X`, `2X`, `4X`, `8X`, `16X`. The
  defaults are `2X`, `16X` and `1X`.
- **gas_resistance** (*Optional*): [Sensor](https://esphome.io/components/sensor/) publishing the last valid gas
  resistance of the measurement in Ω.
- **heater** (*Optional*): gas heater configuration, see below.

#### Heater

In `forced` mode the heater is described by a single temperature and duration:

- **temperature** (*Optional*, int): heater plate temperature in °C, at most 400. Defaults to `320`.
- **duration** (*Optional*, [Time](https://esphome.io/guides/configuration-types#config-time)): heating duration before
  every measurement. Defaults to `150ms`.

In `parallel` and `sequential` mode the heater sweeps through a **profile** of up to ten steps instead. Every step
takes a **temperature** and an optional **gas_resistance** sensor of its own, plus:

- **duration** (*sequential* mode, required): how long this step heats for.
- **multiplier** (*parallel* mode, required): how many shared heater durations this step lasts, between 1 and 255.

Parallel mode additionally takes a **shared_duration** (defaults to `140ms`), the duration of a single profile step.
The time the temperature, pressure and humidity measurement takes is subtracted from it automatically to get the shared
heater duration the sensor expects.

### Examples

- [`examples/forced_mode.yaml`](examples/forced_mode.yaml) - one measurement per update interval, the sensor sleeps in
  between.
- [`examples/parallel_mode.yaml`](examples/parallel_mode.yaml) - the gas sensor sweeps through a heater profile while
  temperature, pressure and humidity are measured continuously.
- [`examples/sequential_mode.yaml`](examples/sequential_mode.yaml) - the sensor steps through the heater profile on its
  own, sleeping between the measurements.

All examples are written for a generic ESP32 board (`esp32dev`) with the sensor on `GPIO21`/`GPIO22`.

### Development

Run the following before committing to auto-format the component against ESPHome's own style:

```sh
pip install clang-format==13.0.1
find components \( -name "*.cpp" -o -name "*.h" \) -not -name "bme69x*" | xargs clang-format -i
```

The vendored Bosch BME69x Sensor API (`bme69x*`) is kept unmodified and is excluded from formatting. CI runs the same
check on every push/PR via `.github/workflows/format_check.yml` and fails on unformatted code.
`.github/workflows/build.yml` compiles every YAML under `examples/` on every push/PR.

### Hardware design

You can find hardware design for this board in the _BME690 breakout board_ hardware repository.

### Documentation

Access library documentation [here](https://docs.soldered.com/).

### About Soldered

<img src="https://raw.githubusercontent.com/SolderedElectronics/Soldered-Generic-Arduino-Library/dev/extras/Soldered-logo-color.png" alt="soldered-logo" width="500"/>

At Soldered, we design and manufacture a wide selection of electronic products to help you turn your ideas into acts and bring you one step closer to your final project. Our products are intented for makers and crafted in-house by our experienced team in Osijek, Croatia. We believe that sharing is a crucial element for improvement and innovation, and we work hard to stay connected with all our makers regardless of their skill or experience level. Therefore, all our products are open-source. Finally, we always have your back. If you face any problem concerning either your shopping experience or your electronics project, our team will help you deal with it, offering efficient customer service and cost-free technical support anytime. Some of those might be useful for you:

- [Web Store](https://www.soldered.com/shop)
- [Tutorials & Projects](https://soldered.com/learn)
- [Documentation](https://docs.soldered.com)

### Open-source license

Soldered invests vast amounts of time into hardware & software for these products, which are all open-source. Please support future development by buying one of our products.

Check license details in the LICENSE file. Long story short, use these open-source files for any purpose you want to, as long as you apply the same open-source licence to it and disclose the original source. No warranty - all designs in this repository are distributed in the hope that they will be useful, but without any warranty. They are provided "AS IS", therefore without warranty of any kind, either expressed or implied. The entire quality and performance of what you do with the contents of this repository are your responsibility. In no event, Soldered (TAVU) will be liable for your damages, losses, including any general, special, incidental or consequential damage arising out of the use or inability to use the contents of this repository.

## Have fun!

And thank you from your fellow makers at Soldered Electronics.
