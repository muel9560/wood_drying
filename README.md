# Black Walnut Air-Drying Moisture Monitor
## Project Documentation

---

## Overview

A three-node ESP32-S3 system using pin-type resistance measurement to monitor
moisture content (MC%) across a black walnut air-drying stack in a garage
environment. Each node drives two CD74HC4067 16-channel analog multiplexers
for up to 16 probe pairs per node (32 channels per node, 96 total across all
three nodes).

Sensor data reports via StatsD → InfluxDB, matching the existing
temperature/humidity sensor infrastructure already in place.

---

## Stack Layout

### 5/4 and 6/4 Stock — Node 1

Three Husky 4-shelf units (77"W × 72"H × 24"D) arranged in a row with 18"
gaps between units, giving approximately 9 feet of board run across three
24" shelf sections.

- **6/4 stock** on the bottom two shelves (heavier, more drying risk, more
  buffered from ceiling temperature swings)
- **5/4 stock** on the top two shelves

Wire shelves provide natural airflow underneath each board layer. Stickers
(spacers) are still required between board layers within each shelf tier.
Stickers must align vertically through the stack to transfer load to the
shelf frame.

### 8/4 Stock — Node 2

Dedicated stack monitored with two MUX boards. Up to 5 slabs with 2 shallow
+ 2 deep probes per slab (4 channels per slab, 20 channels for 5 slabs).

### 10/4 Stock — Node 3

Same configuration as Node 2. Highest case-hardening risk — monitor
shell-to-core differential closely. If shallow vs. deep MC% difference
exceeds 5%, tighten the lumber wrap immediately.

---

## Hardware Bill of Materials

| Item | Qty | Source |
|---|---|---|
| ESP32-S3 DevKit | 3 | On hand |
| DEVMO CD74HC4067 2-pack breakout | 3 packs (6 boards) | Amazon |
| BOJACK 1MΩ resistor (from assortment) | 6 | Amazon |
| 100nF ceramic cap (from assortment) | 6 | Amazon |
| ER308L stainless TIG wire 1/16" 30-pack | 1 | Amazon |
| Fermerry 22 AWG stranded wire 6-color | 1 | Amazon |
| Chazcool 2-pin screw terminals 50-pack | 1 | Amazon |
| BOJACK breadboard + jumper wire kit | 1 | Amazon |
| FNIRSI DMT-99 multimeter | 1 | Amazon |
| Heat shrink assortment (red/black) | On hand | — |
| Husky 4-shelf unit 77"×72"×24" | 3 | Home Depot |

---

## Wiring — All Three Nodes Identical

| ESP32-S3 GPIO | Function | Destination |
|---|---|---|
| GPIO1 | ADC Input | Voltage divider midpoint (1MΩ to GND) |
| GPIO2 | Excitation | COM pin of both MUX A and MUX B |
| GPIO4 | MUX S0 | S0 pin on both MUX A and MUX B (parallel) |
| GPIO5 | MUX S1 | S1 pin on both MUX A and MUX B (parallel) |
| GPIO6 | MUX S2 | S2 pin on both MUX A and MUX B (parallel) |
| GPIO7 | MUX S3 | S3 pin on both MUX A and MUX B (parallel) |
| GPIO8 | MUX A INH | INH pin of MUX A (active LOW to enable) |
| GPIO9 | MUX B INH | INH pin of MUX B (active LOW to enable) |
| 3.3V | Power | VCC on both MUX boards |
| GND | Ground | VEE and GND on both MUX boards + all common probe pins |

### Passive Components

- **1MΩ resistor**: between GPIO2 (excitation) and GPIO1 (ADC). The ADC
  midpoint also connects to GND via 10kΩ to prevent floating when probes
  are open circuit.
- **100nF ceramic cap**: across VCC/GND on each MUX, placed physically
  close to the chip to suppress switching noise.

### MUX Channel Selection Logic

S0–S3 are wired to both MUX boards in parallel. The active board is
selected by pulling its INH pin LOW while keeping the other HIGH:

- Reading MUX A channel: GPIO8 LOW, GPIO9 HIGH
- Reading MUX B channel: GPIO8 HIGH, GPIO9 LOW

---

## Probe Construction

### Materials

- ER308L stainless TIG welding wire 1/16" diameter, cut to length
- 22 AWG stranded hookup wire, two colors per probe pair
- Heat shrink tubing (red for + probe, black for – probe)

### Probe Anatomy

```
    ┌──────────────────────┐  ← wood surface
    │   HEAT SHRINK        │  ← insulates shaft above entry point
    │   + solder joint     │  ← hookup wire attached here
    │   (above surface)    │
    ├──────────────────────┤  ← entry point / depth stop mark
    │   BARE WIRE          │  ← active sensing zone
    │   (inside wood)      │
    └──────────────────────┘  ← probe tip
```

The depth stop is marked with a permanent marker or wrap of electrical tape
before insertion. Pre-applying heat shrink above the mark also acts as a
natural stop.

### Insertion Method

1. Pre-drill pilot hole with 1/16" drill bit
2. Push/twist wire in by hand or with pliers
3. Do NOT hammer — the wire will deflect or bend

### Probe Lengths by Thickness

| Stock | Shallow insert depth | Deep insert depth | Probe total length (shallow) | Probe total length (deep) |
|---|---|---|---|---|
| 5/4 (~1.25") | 5/16" | 5/8" | 1.5" | 1.9" |
| 6/4 (~1.5") | 3/8" | 3/4" | 1.5" | 1.9" |
| 8/4 (~2.0") | 1/2" | 1" | 1.5" | 2.25" |
| 10/4 (~2.5") | 5/8" | 1-1/4" | 1.5" | 2.75" |

Total length = ~1.25" above-surface stub + insertion depth. The above-surface
stub is where the hookup wire solders to the probe.

### Probe Placement in Each Board

- **Width**: center of the board width
- **Length**: one pair at 1/3 board length, second pair at 2/3 board length
- **Probe pair spacing**: 1 inch apart, along the grain (not across)
- **End grain exclusion**: minimum 12" from each end — end grain dries
  10–15× faster and gives unrepresentative readings

---

## Calibration

### Resistance to MC% Conversion (Black Walnut)

Logarithmic curve with black walnut species correction (+2.5% over
Douglas fir baseline):

```
MC% = 31.62 - 2.985 × log₁₀(R_ohms)
```

Clamped: below 8,000Ω returns 28% (fiber saturation); above 1,500,000Ω
returns 6% (below reliable measurement range).

### Temperature Correction

Calibration baseline is 70°F (21.1°C). Applied correction:

```
MC_corrected = MC - ((T_celsius - 21.1) / 5.6) × 0.5
```

Temperature is pulled from the existing stack temperature sensor via
Home Assistant entity.

### Reference Lookup Table

| Resistance | Approx MC% (black walnut) |
|---|---|
| >1,500,000 Ω | <7% |
| 500,000 Ω | ~8–9% |
| 100,000 Ω | ~12–13% |
| 50,000 Ω | ~16% |
| 20,000 Ω | ~20–22% |
| 10,000 Ω | ~25–28% |

### Field Calibration Procedure

1. Assemble circuit and flash ESPHome firmware
2. Insert probe pair into a test board
3. Read voltage at GPIO1 (ADC midpoint) with multimeter
4. Verify calculated resistance matches multimeter reading across probes
5. Compare MC% output to a known commercial meter on the same board
6. Adjust `cal_a` (31.62) and `cal_b` (2.985) substitutions in YAML to match

---

## ESPHome Configuration Files

| File | Node | Stock | MUX A | MUX B |
|---|---|---|---|---|
| `wood_moisture_node1.yaml` | Node 1 | 5/4 & 6/4 | 5/4 upper shelves (12 ch) | 6/4 lower shelves (12 ch) |
| `wood_moisture_node2.yaml` | Node 2 | 8/4 | Slabs 1–3 (12 ch) | Slabs 4–5 (8 ch) |
| `wood_moisture_node3.yaml` | Node 3 | 10/4 | Slabs 1–3 (12 ch) | Slabs 4–5 (8 ch) |

### Required secrets.yaml Entries

```yaml
wifi_ssid: "YourSSID"
wifi_password: "YourWifiPassword"
ap_fallback_password: "fallback123"
api_encryption_key: "base64-32-byte-key"
ota_password: "your-ota-password"
statsd_host: "192.168.x.x"
```

### Before Flashing Each Node

1. Update `sensor.stack_temperature_c` entity ID to match your actual
   Home Assistant temperature sensor for that stack section
2. Fill in `secrets.yaml`
3. Rename sensor `name` fields to match physical slab/board positions
   once the stack is assembled

---

## InfluxDB / Grafana Suggestions

Since all moisture data flows via StatsD → InfluxDB alongside your existing
temp/humidity data, useful derived metrics to build:

- **Per-board MC% over time** — trend lines showing drying progress
- **Shell vs. core differential** (Shallow minus Deep per probe pair) —
  primary case-hardening risk indicator
- **MC% vs. ambient RH correlation** — tune wrap open/close decisions
- **Estimated days to target MC** — linear regression on drying curve
  after 2–3 weeks of data
- **Alert threshold**: flag any channel where Shallow−Deep > 5% MC

Target final MC% for interior woodworking in Boise, Idaho: **6–8%**

---

## Drying Rate Guidelines (Air Drying)

The rule of thumb for air drying is 1 year per inch of thickness, but this
varies significantly with species, temperature, and humidity control.

| Stock | Estimated air-dry time (garage, controlled wrap) |
|---|---|
| 5/4 | 6–9 months |
| 6/4 | 9–12 months |
| 8/4 | 14–20 months |
| 10/4 | 20–30 months |

Black walnut dries relatively well compared to other hardwoods but is
prone to surface checking if the shell dries faster than ~1% MC per week
in the early stages (first 2–3 months while MC is above 20%).

---

## Files in This Package

| File | Description |
|---|---|
| `wood_moisture_project.md` | This documentation |
| `wood_moisture_node1.yaml` | ESPHome config — Node 1 (5/4 & 6/4) |
| `wood_moisture_node2.yaml` | ESPHome config — Node 2 (8/4) |
| `wood_moisture_node3.yaml` | ESPHome config — Node 3 (10/4) |
| `moisture_sensor_wiring_3node.png` | Wiring diagram — all three nodes |
| `moisture_sensor_wiring.png` | Original single-node breadboard reference |
