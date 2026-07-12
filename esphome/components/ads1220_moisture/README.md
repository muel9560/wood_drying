# `ads1220_moisture` — ESPHome external component

24-bit ratiometric constant-current front end for the wood-resistance probes.
See [`../../wood_moisture_ads1220_frontend.md`](../../wood_moisture_ads1220_frontend.md)
for the circuit and the chip-choice guidance.

> **Status: DRAFT.** The ADS1220 register values and conversion timing follow the
> datasheet but are **not yet hardware-validated**. Bench-check against known
> resistors (e.g. 10 k, 100 k, 1 M ±0.1%) before trusting readings.

## What it does

- Configures the ADS1220 for ratiometric mode (external ref across `R_ref`, IDAC
  excitation), single-shot, 20 SPS with 50/60 Hz rejection.
- Per channel: sets the external mux address, does a gain-1 estimate, re-reads at
  the highest non-railing PGA gain (the auto-range), computes `R_wood = R_ref ·
  |ratio|`, applies `cal_a/cal_b` + optional °C temp correction, clamps MC to
  [6, 28], and publishes `mc` (+ optional raw `resistance`).
- An open probe / `R_wood ≥ R_ref` rails at gain 1 → publishes `NAN`.

## Wiring

- ADS1220 on the SPI bus; `cs_pin` + optional `drdy_pin` (recommended — faster and
  more reliable than the fixed delay).
- `IDAC1 → AIN0` (mux common / probe top), `AIN1 → node_mid` tied to `REFP0`,
  `R_ref` from `REFP0` to `REFN0`/GND.
- `mux_pins` (S0–S3) + `mux_enable_pin` drive the CD74HC4067 exactly as the current
  design does. Omit them for a direct 1-channel bench setup.

## Example

```yaml
external_components:
  - source:
      type: local
      path: components
    components: [ads1220_moisture]

spi:
  clk_pin: GPIO12
  mosi_pin: GPIO11
  miso_pin: GPIO13

ads1220_moisture:
  id: afe
  cs_pin: GPIO10
  drdy_pin: GPIO14
  r_ref: 100000        # the 0.1% reference resistor, ohms
  idac: 10uA           # 10/50/100/250/500/1000/1500 uA
  mux_pins: [GPIO4, GPIO5, GPIO6, GPIO7]   # CD74HC4067 S0-S3 (omit if no mux)
  mux_enable_pin: GPIO8

sensor:
  - platform: homeassistant
    id: stack_temp_c
    entity_id: sensor.woodstack_temp_c     # ambient in °C

  - platform: ads1220_moisture
    ads1220_moisture_id: afe
    name: "5-4 b01 mid shallow"
    channel: 0
    cal_a: 45.8
    cal_b: 7.53
    temperature: stack_temp_c
    resistance:
      name: "5-4 b01 mid shallow R"
    update_interval: 300s
```

## ADS124S08 / S06 variant

Same measurement layer, different register/mux map — it's a separate component
(`ads124s08_moisture`) I'll write once channel counts are settled. The differences:

- ~18 registers vs 4; the mux is `INPMUX` (any AINx as P/N) instead of a 4-bit code.
- Dedicated `REFP0/REFN0` pins for `R_ref` (no input pin consumed).
- Up to ~11 probe channels use the *internal* mux — drop the CD74HC4067 and the
  `mux_pins`; `channel` selects `INPMUX` directly.
- Burnout current sources give a probe-break flag worth surfacing as a sensor.

The `MoistureChannel` math (autorange → ratio → R → MC) is identical and can be
shared.
