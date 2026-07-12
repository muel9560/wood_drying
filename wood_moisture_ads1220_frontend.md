# ADS1220 / ADS124S08 Analog Front-End Option

A 24-bit, auto-ranging analog front end for the moisture probes — the same thing
the PSoC 5LP would have done (IDAC current excitation + PGA + ΔΣ ADC), as a single
SPI chip on the existing ESP32-S3 node, staying inside ESPHome / StatsD / InfluxDB.

## Why

The dry-end problem is the ESP32-S3's 12-bit ADC noise floor: at ~8% MC the
divider node is ~0.2 V, which is ~1–2 usable bits. These TI ΔΣ ADCs replace that
read with **24-bit resolution + a PGA (gain 1–128) + two matched current sources
(IDAC)**. In the 7–14% "is it ready" window you go from ~2 usable bits to ~16+.

## Measurement topology — ratiometric constant-current

```
IDAC ─▶ node_top ─[ R_wood ]─ node_mid ─[ R_ref ]─ GND
        AINP = node_top, AINN = node_mid   →  V_wood = I·R_wood
        REFP = node_mid, REFN = GND         →  Vref   = I·R_ref
        ADC code  ∝  V_wood / Vref  =  R_wood / R_ref
```

- **The current cancels** — code depends only on `R_wood / R_ref`, so IDAC
  absolute accuracy and drift don't matter: `R_wood = R_ref · (code/FS) / gain`.
  Reference noise cancels too (same current through both).
- **R_ref is the calibration anchor** — one 0.1 % / 25 ppm resistor (100 kΩ).
- Putting R_ref *below* the wood elevates the common mode (~I·R_ref ≈ 1 V), which
  keeps the PGA in range at high gain.

## Auto-ranging — just the PGA

With **R_ref = 100 kΩ, IDAC = 10 µA** (Vref ≈ 1 V) a single fixed setup + PGA gain
covers the useful range; firmware reads once at gain 1 to estimate the ratio, then
re-reads at the highest non-railing gain:

| R_wood | ≈ MC% | gain |
|---|---|---|
| 8 kΩ | 28 | 8–16 |
| 20 kΩ | ~22 | 4 |
| 50 kΩ | ~16 | 2 |
| 100 kΩ | ~7.6 | 1 (ceiling) |

**Anti-polarization AC:** alternate the current direction between conversions and
average — cancels electrode DC and ADC offset. Full current-reversal in this
ratiometric topology needs either careful IDAC re-routing or an external
polarity-reversal switch on the probe; the firmware draft measures single-direction
with proper settling and leaves chopping as a documented v2 (see the component).

## Scope / honest limits

- **Covers ~28 %–~7.5 % MC**, clean and auto-ranged, at true 24-bit — the entire
  furniture-drying decision range. The 7–9 % zone that was noise on the ESP32
  becomes solid.
- **Below ~7.5 % still saturates** — that's the IDAC 10 µA floor against the wood's
  rising resistance (compliance), i.e. physics, not the chip. Bone-dry is
  unreachable by resistance sensing regardless.

## Chip choice — by per-node channel count

Same measurement engine on all three; the difference is the input mux and channel
count. A 2-wire ratiometric probe uses one input + a shared return bus; the
reference resistor sits on dedicated REF pins (S06/S08) so it doesn't cost a
channel.

| | **ADS1220** | **ADS124S06** | **ADS124S08** |
|---|---|---|---|
| Analog inputs | 4 | 6 | 12 |
| Probe channels (shared-return) | ~2–3 | ~5 | ~11 |
| Dedicated reference pins | no | yes | yes |
| Probe-break / burnout detect | limited | yes | yes |
| To reach 16 ch/node | **needs external CD74HC4067** | + small mux | + small mux / 2 chips |
| Package | TSSOP-16 (hand-solderable) | TQFP-32 | TQFP-32 |
| Price | ~$5 | ~$8 | ~$10–12 |

| node probe count | pick | why |
|---|---|---|
| ≤ 5 | ADS124S06 | cheapest, no mux, break-detect |
| 6–11 | ADS124S08 | no external mux, break-detect, one clean board |
| 12–16+ | ADS1220 + CD74HC4067 16-mux | uniform, mux already designed |

The **shelving node** (5/4 + 6/4, ~5 boards each × shallow/deep ≈ up to ~20 ch) is
over 11 → **ADS1220 + the existing 16-mux**. The **slab nodes** (8/4+, a few slabs
× 2 probe-sets × 2 depths ≈ 4–8 ch) fit on **one ADS124S08, no mux**, and gain
probe-break alerts.

**Leakage note:** both S06/S08 internal muxes are far lower-leakage than the cheap
CD74HC4067, so moving to them *reduces* the off-channel shunt that drove the
original dual-MUX redesign — a bonus, not a risk.

## What changes on the board

**Keep:** ESP32-S3, the CD74HC4067 mux (only where a node exceeds the ADC's inputs),
the probes, and the entire ESPHome → StatsD → Grafana pipeline.

**Add per node:**

| item | qty | note |
|---|---|---|
| ADS1220 or ADS124S06/S08 (or a breakout) | 1 | see chip choice |
| R_ref 100 kΩ, **0.1 % / 25 ppm** | 1 | the calibration anchor |
| input RC filter (1 kΩ + 1–10 nF C0G) per input | few | antialias + settling |
| optional analog switch (74HC4066) | 0–1 | only for external polarity reversal |

**Wiring:** the ADC talks SPI to the ESP32 (SCLK / DIN / DOUT / CS + DRDY). IDAC
output and the sense inputs go to the mux commons + R_ref; the ESP32 keeps driving
the mux address exactly as now.

## Firmware

ESPHome has no built-in component for these parts, so the front end is a **custom
external component**: [`esphome/components/ads1220_moisture/`](esphome/components/ads1220_moisture/).
It configures the ADC for ratiometric IDAC mode, does the per-channel PGA
autorange, computes `R_wood`, applies the `cal_a`/`cal_b` curve + temp correction,
and publishes `mc` + `resistance` under the existing StatsD schema. The draft
targets the **ADS1220**; the ADS124S08 is the same measurement layer over a
different register/mux map (noted in the component README) and drops in once you've
settled channel counts.

> **Status: draft.** The register values and conversion timing need bench
> validation against a real chip before the readings are trustworthy.

## Migration

Nothing about the probes, calibration curve, or data pipeline changes — this only
swaps the *sense path* (ESP32 ADC → ADS1220). You can pilot it on one bench channel
alongside the existing divider read and compare directly before converting a node.
