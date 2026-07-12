# `ads1220_moisture` — ESPHome external component

24-bit ratiometric constant-current front end for the wood-resistance probes.
Circuit and part choice: [`../../wood_moisture_ads1220_frontend.md`](../../wood_moisture_ads1220_frontend.md).
Board and the reasoning behind R_ref / AVDD: `docs/analog-front-end-design.md` in the
**wood-sensor-ads** repo.

> **Status: not hardware-validated.** The register values follow the datasheet and the
> C++ compiles, but no reading has ever come off a real chip. **Do
> [`BENCH.md`](BENCH.md) first** — it takes ten minutes and one resistor.

## What it does

- Configures the ADS1220 for ratiometric mode: external reference across `R_ref`, IDAC
  excitation out of AIN0, single-shot, 20 SPS, mains rejection.
- Per channel: sets the external mux address, takes a gain-1 estimate, re-reads at the
  highest non-railing PGA gain, computes `R_wood = R_ref · |ratio|`, applies
  `cal_a`/`cal_b` + optional °C correction, clamps MC to [6, 28], publishes `mc` and
  (optionally) the raw `resistance`.
- An open probe, or `R_wood ≥ R_ref`, rails at gain 1 → publishes `NAN`.

## Wiring

- ADS1220 on the SPI bus; `cs_pin` + `drdy_pin` (strongly recommended).
- `IDAC1 → AIN0` (mux common / probe top). `AIN1` and `REFP0` both to the probe return
  bus. `R_ref` from that bus to `REFN0`/AGND.
- `mux_pins` (S0–S3) + `mux_enable_pin` drive the CD74HC4067. Omit them for a
  single-channel bench setup.

**The probe return bus is not ground.** It sits at `I·R_ref` ≈ 2.0 V.

**Two things the breakout does not do for you** (measured, not assumed): it does not bond
AVSS to DGND, and it does not ground CLK. Both must be done on your board. A floating
CLK is a dead ADC.

## Options

| option | default | notes |
|---|---|---|
| `r_ref` | `200000` | The 0.1% reference. **Full scale at gain 1 is exactly `R_wood == r_ref`**, so this is also the dry-end ceiling: 200 k reaches ~5.9% MC, 100 k rails at ~8.2% — inside the range you care about. Measure your actual resistor and use its real value. |
| `idac` | `10uA` | The minimum. Larger currents lower the ceiling (compliance is `AVDD − 0.9 V`). |
| `line_frequency` | `60Hz` | `60Hz` / `50Hz` / `50/60Hz` / `off`. Rejecting one frequency buys 105 dB against 90 dB for both — pick the one you actually have. |
| `mux_settle_ms` | `50` | After a channel switch, before the first conversion. |
| `drdy_pin` | — | Optional but recommended; without it the component waits a fixed 80 ms per conversion. |

## Design notes

**It does not block.** A reading is a settle delay plus one or two ~50 ms conversions.
All sixteen channels share an `update_interval` and fire in the same loop iteration, so
doing that synchronously would stall the main loop for seconds and drop the API
connection. Channels only *queue* a request; the hub works through the queue from
`loop()` as a state machine and pushes each result back when it lands.

**It parks the mux between readings.** The IDACs stay alive between single-shot
conversions, so leaving the last channel selected would push 10 µA through that probe
for the whole `update_interval` and DC-polarise the electrodes. `EN` goes high when a
reading finishes.

**It verifies its own registers.** `setup()` writes the config and reads it straight
back; on a mismatch it logs the expected vs actual byte and marks the component failed.
A mis-wired SPI bus then looks like a mis-wired SPI bus, instead of producing readings
that look plausible but are noise.

## Example

```yaml
external_components:
  - source:
      type: git
      url: https://github.com/muel9560/wood_drying
      ref: feature_ads1220_frontend
    components: [ads1220_moisture]

spi:                     # XIAO ESP32-S3
  clk_pin: GPIO7
  miso_pin: GPIO8
  mosi_pin: GPIO9

ads1220_moisture:
  id: afe
  cs_pin: GPIO1
  drdy_pin: GPIO2
  r_ref: 200000
  idac: 10uA
  line_frequency: 60Hz
  mux_pins: [GPIO3, GPIO4, GPIO5, GPIO6]   # via the 74AHCT541 level shifter
  mux_enable_pin: GPIO43

sensor:
  - platform: homeassistant
    id: stack_temp_c
    entity_id: sensor.woodstack_temp_c
    internal: true

  - platform: ads1220_moisture
    ads1220_moisture_id: afe
    name: "5-4 b01 mid shallow"
    channel: 0
    cal_a: 45.8          # oven-dry refit for this walnut, NOT the USDA 31.62/2.985
    cal_b: 7.53
    temperature: stack_temp_c
    resistance:
      name: "5-4 b01 mid shallow R"
    update_interval: 300s
```

Full node: [`../../wood_moisture_node1_ads1220.yaml`](../../wood_moisture_node1_ads1220.yaml).
Bench rig: [`../../ads1220_bench.yaml`](../../ads1220_bench.yaml).

## ADS124S08 / S06 variant

Same measurement layer, different register/mux map — a separate component
(`ads124s08_moisture`) once channel counts settle. The differences: ~18 registers vs 4;
`INPMUX` instead of a 4-bit code; dedicated `REFP0/REFN0` so `R_ref` costs no input; up
to ~11 probes on the *internal* mux (drop the CD74HC4067, the 74AHCT541 and the
`mux_pins`); burnout current sources give a probe-break flag worth surfacing.

The `MoistureChannel` maths (autorange → ratio → R → MC) is identical and can be shared.
