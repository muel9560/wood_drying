# Black Walnut Air-Drying Moisture Monitor
## Project Documentation — Corrected Dual-MUX Design

## Overview

A multi-node ESP32-S3 system using pin-type resistance measurement to monitor
moisture content (MC%) across a black walnut air-drying stack. Each node uses
two CD74HC4067 16-channel analog multiplexers where the wood resistance bridges
MUX-A output (excitation) to MUX-B input (ADC sense).

## Corrected Circuit — Two Separate Nodes

Node A and Node B are NEVER directly connected. The only path between them
is through the wood.

**Node A (Excitation):** GPIO9 → R1 (10kΩ) → MUX-A SIG → selected channel → Probe A+

**Node B (ADC Sense):** Probe B+ → MUX-B selected channel → MUX-B SIG → GPIO1 (ADC) + R2 (100kΩ) → GND

**Signal path:**
GPIO9 → R1 → MUX-A SIG → C_n → Probe A+ → [R_wood] → Probe B+ → C_n → MUX-B SIG → GPIO1 / R2 → GND

**Formula:** V = VCC × R2 / (R1 + R_wood + R2)
**Solve:** R_wood = (R2 × VCC / V) - R2 - R1

Higher moisture = lower R_wood = HIGHER voltage at GPIO1
Lower moisture = higher R_wood = LOWER voltage at GPIO1

## GPIO Assignments

| GPIO | XIAO Pad | Function | Destination |
|---|---|---|---|
| GPIO1 | D0 | ADC input | MUX-B SIG + R2 top (Node B) |
| GPIO9 | D10 | Excitation | R1 top (Node A only) |
| GPIO4 | D3 | S0 | Both MUX S0 |
| GPIO5 | D4 | S1 | Both MUX S1 |
| GPIO6 | D5 | S2 | Both MUX S2 |
| GPIO7 | D8 | S3 | Both MUX S3 |
| GPIO8 | D9 | EN | Both MUX EN |
| 3V3 | 3V3 | Power | Both MUX VCC |
| GND | GND | Ground | Both MUX GND + R2 bottom |

## Passive Components

| Component | Value | Node | Connects |
|---|---|---|---|
| R1 | 10kΩ | A | GPIO9 → R1 → MUX-A SIG |
| R2 | 100kΩ | B | MUX-B SIG + GPIO1 → R2 → GND |
| C1 | 100nF | — | MUX-A VCC to GND |
| C2 | 100nF | — | MUX-B VCC to GND |

## Expected ADC Voltage at GPIO1

| Wood condition | R_wood | V at GPIO1 | MC% |
|---|---|---|---|
| Short (probes touching) | 0Ω | 3.00V | 28% clamp |
| Very wet (>30%) | 5kΩ | 2.87V | 28% clamp |
| 28% MC | 8kΩ | 2.80V | 28% |
| 25% MC | 20kΩ | 2.54V | 25% |
| 20% MC | 35kΩ | 2.28V | 20% |
| 16% MC | 50kΩ | 2.06V | 16% |
| 13% MC | 100kΩ | 1.57V | 13% |
| 9% MC | 300kΩ | 0.80V | 9% |
| 8% MC | 500kΩ | 0.54V | 8% |
| 7% MC | 750kΩ | 0.38V | 7% |
| 6% MC | 1.5MΩ | 0.21V | 6% clamp |
| Open circuit | ∞ | ~0V | NAN |

## Unpowered Validation — Multimeter Checks

| # | Measure between | Expected | Failure means |
|---|---|---|---|
| 1 | R1 leads in place | 10kΩ | Wrong resistor |
| 2 | R2 leads in place | 100kΩ | Wrong resistor |
| 3 | GPIO9 pad → MUX-A SIG | 10kΩ | R1 not in circuit |
| 4 | MUX-A SIG → MUX-B SIG | OPEN (OL) | Nodes shorted — CRITICAL |
| 5 | MUX-B SIG → GND | 100kΩ | R2 issue |
| 6 | GPIO1 pad → GND | 100kΩ | R2 or GPIO1 wire |
| 7 | GPIO1 pad → MUX-B SIG | 0Ω continuity | Must be connected |
| 8 | GPIO9 pad → MUX-B SIG | OPEN (OL) | Excite leaking to sense |
| 9 | GPIO9 pad → GND | OPEN (OL) | R1 shorted to sense |
| 10 | VCC bus → GND bus | >1MΩ | Power short |
| 11 | MUX-A C0 → MUX-B C0 | OPEN (OL) | Channel pins crossed |
| 12 | Across probe pair in wood | 8kΩ–1.5MΩ | Probe contact |

## Powered Validation Sequence

| # | Measurement | Expected | Failure means |
|---|---|---|---|
| P1 | VCC on MUX-A | 3.28–3.32V | Power wiring |
| P2 | VCC on MUX-B | 3.28–3.32V | Power wiring |
| P3 | EN on MUX-A | <0.1V | EN not LOW |
| P4 | EN on MUX-B | <0.1V | EN not LOW |
| P5 | GPIO9 during excite ON | 3.28–3.32V | Pin not driving |
| P6 | MUX-A SIG during excite (no probe) | ~3.2V | R1 or SIG issue |
| P7 | GPIO1 with open probe | <0.05V | R2 pull-down working |
| P8 | GPIO1 with probes in wet wood | 1.5–2.8V | Full path working |
| P9 | GPIO1 with probes in dry wood | 0.2–0.8V | Full path + dry range |

## Probe Wiring

Each probe pair has two pins — one connects to MUX-A channel, the other to
the SAME channel number on MUX-B. No common GND wire at the probe.

| Terminal | Wire 1 | Wire 2 |
|---|---|---|
| CH0 | Probe A+ → MUX-A C0 | Probe B+ → MUX-B C0 |
| CH1 | Probe A+ → MUX-A C1 | Probe B+ → MUX-B C1 |
| etc. | ... | ... |

## Probe Lengths

| Nominal | Actual | Shallow | Deep | Shallow total | Deep total |
|---|---|---|---|---|---|
| 5/4 | ~1.25" | 5/16" | 5/8" | 1-9/16" | 1-7/8" |
| 6/4 | ~1.50" | 3/8" | 3/4" | 1-5/8" | 2" |
| 7/4 | ~1.75" | 7/16" | 7/8" | 1-11/16" | 2-1/8" |
| 8/4 | ~2.00" | 1/2" | 1" | 1-3/4" | 2-1/4" |
| 9/4 | ~2.25" | 9/16" | 1-1/8" | 1-13/16" | 2-3/8" |
| 10/4 | ~2.50" | 5/8" | 1-1/4" | 1-7/8" | 2-1/2" |
| 11/4 | ~2.75" | 11/16" | 1-3/8" | 1-15/16" | 2-5/8" |
| 12/4 | ~3.00" | 3/4" | 1-1/2" | 2" | 2-3/4" |
| 13/4 | ~3.25" | 13/16" | 1-5/8" | 2-1/16" | 2-7/8" |

## Drying Times

| Nominal | Time to ~8% MC |
|---|---|
| 5/4 | 6–9 months |
| 6/4 | 9–12 months |
| 7/4 | 10–14 months |
| 8/4 | 14–20 months |
| 9/4 | 18–24 months |
| 10/4 | 20–30 months |
| 11/4 | 24–36 months |
| 12/4 | 28–40 months |
| 13/4 | 36–48+ months |

Target MC% for Boise, Idaho: 6–8%

## Calibration

MC% = 31.62 - 2.985 × log₁₀(R_wood)
R_wood = (100000 × 3.3 / V) - 100000 - 10000
Temp correction: MC -= ((T_celsius - 21.1) / 5.6) × 0.5

## ESPHome Substitutions

```yaml
substitutions:
  r1_value: "10000.0"
  r2_value: "100000.0"
  cal_a: "31.62"
  cal_b: "2.985"
  excite_settle_delay: "250"
  baseline_delay: "250"
  mux_settle_delay: "10"
  scan_interval: "30s"
```
