# Wood Moisture Monitor — Design Update
## Dual MUX Fix, PSoC 5LP Option, and Probe Clip Solution

---

## 1. The Problem with the Original Single-MUX Design

The original design routed both the excitation signal (GPIO2) and the ADC
reading (GPIO1) through the same SIG/COM pin on a single CD74HC4067. This
causes a measurement error because:

The CD74HC4067 has leakage current on all unselected channels — typically
100–500 nA per channel at 3.3V. With 15 unselected channels, that leakage
appears on the SIG line as a small but real voltage offset. When excitation
and ADC sense share the same SIG pin, this leakage directly corrupts the
resistance measurement. At the high resistances representing dry wood
(500kΩ–1.5MΩ), even small leakage errors shift the calculated MC% by
several percent.

---

## 2. The Dual MUX Fix

Use two CD74HC4067 boards per node, enabled simultaneously with the same
S0–S3 address:

- **MUX-A**: GPIO2 (excitation) → SIG. Drives the excitation signal out
  through the selected channel to one probe pin.
- **MUX-B**: SIG → GPIO1 (ADC). Reads the voltage at the selected channel
  from the other probe pin.

Both boards select the same channel number at the same time. The probe pair
for that channel receives excitation on one pin and the ADC reads the
response on the other. Leakage from unselected channels on each MUX now
appears as common-mode noise rather than a direct measurement error, because
the excitation and sense paths are separated.

### What Changes in the Wiring

- You now use **2 MUX boards per node** instead of 1 — you have 4 packs
  (8 boards total), which is exactly enough for 4 nodes with this design.
- Both boards share the same S0–S3 address lines and the same EN/GPIO8.
- Each probe pair connects to the **same channel number on both boards**:
  probe pin A to MUX-A C-channel, probe pin B to MUX-B C-channel.
- This gives **16 probe pairs** per node (not 32).
- R1 (1MΩ) stays between GPIO2 and MUX-A SIG.
- R2 (10kΩ pull-down) stays between GPIO1 and GND.

### What Does NOT Change

- ESPHome YAML logic is identical — channel selection and AC excitation
  sequence are the same.
- GPIO assignments are unchanged.
- Probe placement and depth in the wood are unchanged.

---

## 3. Probe Clips — Connecting Without Soldering

Since the probes are already cut and placed in the wood without wiring
attached, alligator clips or mini grabber clips are the right solution.

### Recommended Clip Types

**Alligator clips with wire leads** — the easiest option. The jaw grips
the ~1.25" stub of welding rod protruding above the wood surface. Available
at any hardware store or Amazon in packs with 6" or 12" leads. Get them in
two colors (red and black) to keep excitation and return sides distinct.

**Mini grabber / IC test clips** — smaller jaw, better grip on the thin
1/16" welding wire stub. More secure than alligator clips on thin wire.
Search "mini grabber test clip leads" on Amazon — packs of 10 with 20"
leads are common and inexpensive (~$8–12).

### Clip Usage Notes

- One clip per probe stub — clip to the bare welding wire above the wood
  surface, above any electrical tape depth stop mark.
- Run the clip lead back to your screw terminal block.
- Once the system is working and you have time, you can solder hookup wire
  to each stub and apply heat shrink for a permanent connection, then
  remove the clips. The measurement quality is the same either way.
- Label each clip lead with tape flags showing the channel number and
  node — you will have up to 32 leads running around the stack and it
  gets confusing quickly.

---

## 4. PSoC 5LP Design Option

### What Is the PSoC 5LP

The CY8CKIT-059 is a $12–15 prototyping board from Infineon (formerly
Cypress) built around the CY8C5888LTI-LP097 — a 32-bit ARM Cortex-M3
programmable system-on-chip. Unlike a traditional microcontroller, the
PSoC 5LP contains fully configurable analog and digital blocks that can be
wired together in software using the PSoC Creator IDE.

For this application the relevant internal blocks are:

- **AMux (Analog Multiplexer)**: internal mux routing any GPIO to the ADC
  input. Up to 32 channels. Leakage ~1 nA — 100–500x lower than the
  CD74HC4067.
- **Delta-Sigma ADC**: 20-bit resolution with an internal 1.024V ±0.1%
  reference. Compared to the ESP32-S3's 12-bit ADC with ±10% nonlinearity,
  this is a substantial accuracy improvement.
- **IDAC (Current DAC)**: programmable constant-current source for
  excitation. Instead of a voltage divider with GPIO toggle, the IDAC
  drives a known current through the probe and the ADC measures the
  resulting voltage — V = I × R. This is inherently more accurate and
  linear than the voltage divider approach.
- **PGA (Programmable Gain Amplifier)**: boosts the ADC input for
  high-resistance (dry wood) readings, enabling auto-ranging.

### How It Would Work

The PSoC 5LP replaces both the CD74HC4067 MUX boards entirely. Probes
connect directly to PSoC GPIO pins. The PSoC firmware (written in C using
PSoC Creator) selects a channel via the internal AMux, drives the IDAC
excitation, reads the Delta-Sigma ADC, calculates resistance and MC%, then
sends the result over UART to an ESP32-S3 which handles WiFi reporting to
your existing StatsD/InfluxDB stack.

### Accuracy Comparison

| Measurement factor | ESP32-S3 + Dual MUX | PSoC 5LP |
|---|---|---|
| ADC resolution | 12-bit (4,096 steps) | 20-bit (1,048,576 steps) |
| ADC reference accuracy | ~1–2% | ±0.1% |
| MUX leakage | 100–500 nA | ~1 nA |
| Excitation method | Voltage divider (nonlinear) | Constant current IDAC (linear) |
| Estimated MC% accuracy | ±2–3% | ±0.5–1% |

For air-drying lumber the practical question is whether ±2–3% vs ±0.5–1%
matters. At the early stages (MC >20%) it probably doesn't — you're
watching trends, not absolutes. At the dry end (6–9% target MC) where you're
deciding whether the wood is ready for the shop, ±0.5% starts to matter.

### PSoC 5LP Limitations for This Project

- **Programming complexity**: PSoC Creator is a Windows-only IDE with a
  significant learning curve compared to ESPHome YAML. You configure
  hardware blocks graphically then write C firmware.
- **No ESPHome support**: you cannot use ESPHome with the PSoC 5LP. All
  firmware must be written from scratch in C.
- **No WiFi**: the PSoC 5LP has no wireless capability. It needs a
  companion ESP32-S3 to handle WiFi and StatsD reporting via UART.
- **Windows required**: PSoC Creator only runs on Windows. If your dev
  machine is Mac or Linux you would need a VM or separate machine.
- **Longer build time**: this is not a weekend project add-on. Plan 2–4
  weeks to get a working PSoC 5LP node if you haven't used PSoC Creator
  before.

### Verdict

**For right now**: build the dual CD74HC4067 design with ESP32-S3 and
ESPHome. Get the wood stacked and monitored. The ±2–3% accuracy is
sufficient for drying decisions and you can compare readings against a
commercial meter to verify.

**If you want better data later**: the PSoC 5LP is a genuine upgrade,
particularly for the 10/4 stock which will be drying for 2+ years. The
improved dry-end accuracy and lower leakage make a real difference when
you're watching wood approach 7–8% MC. It would be a good winter project
once the stack is established and monitoring.

---

## 5. Immediate Action Plan — Wood Arriving Tomorrow

Given the time pressure, here is the priority order:

1. **Get the wood stacked on the shelves with stickers** — this is the
   most time-critical task. Proper stickering immediately prevents warping
   and early surface checking regardless of monitoring status.

2. **Cover with lumber wrap loosely** — wrap it now even without sensors
   active. A loose wrap is better than no wrap while you finish the
   electronics. You can tighten or vent once sensors are reporting.

3. **Insert probe stubs into pilot holes** — do this before wrapping if
   possible, or unwrap one section at a time. The stubs can sit in the
   wood with no wiring attached without any harm.

4. **Clip leads to probe stubs** — once the breadboard circuit is tested
   and working, run clip leads from the screw terminals out to each probe.
   Do not try to solder in the field right now.

5. **Get Node 1 (5/4 and 6/4) reporting first** — this is the wood you
   have the most of and it is the most straightforward to wire. Nodes 2
   and 3 (8/4 and 10/4) can follow once Node 1 is confirmed working.

The 5/4 and 6/4 stock is relatively forgiving — it can sit uncovered for
24–48 hours without serious damage in a garage environment. The 10/4 stock
is more sensitive but it will not be picking up significant moisture until
it has been stacked for several weeks anyway.

---

## 6. Updated Parts Status

### Already ordered / in hand
- 4 packs × 2 boards = 8 CD74HC4067 breakouts ✓
- Resistors (1MΩ, 10kΩ from BOJACK assortment) ✓
- 100nF ceramic caps ✓
- ER308L probe wire, cut to length ✓
- 22 AWG hookup wire ✓
- Screw terminals ✓
- Breadboard and jumpers ✓
- 3× ESP32-S3 boards for moisture nodes ✓

### Still needed for immediate build
- Mini grabber test clips or alligator clip leads (local store or
  next-day Amazon)

### Optional future upgrade
- CY8CKIT-059 PSoC 5LP board (~$12–15, Amazon or Mouser)
- Windows machine or VM for PSoC Creator IDE (free download)

---

## 7. Updated File List

| File | Description |
|---|---|
| `wood_moisture_design_update.md` | This document |
| `moisture_dual_mux_wiring.png` | Dual MUX wiring diagram (updated design) |
| `moisture_psoc5lp_design.png` | PSoC 5LP design overview and comparison |
| `wood_moisture_node1.yaml` | ESPHome Node 1 — 5/4 & 6/4 (update EN logic) |
| `wood_moisture_node2.yaml` | ESPHome Node 2 — 8/4 |
| `wood_moisture_node3.yaml` | ESPHome Node 3 — 10/4 |
| `moisture_sensor_wiring.png` | Single-node breadboard reference |
| `moisture_sensor_wiring_3node.png` | Three-node overview diagram |
| `moisture_sensor_pinout.png` | Pin connection reference |
| `wood_moisture_project.md` | Full project documentation |

---

## 8. ESPHome YAML Change for Dual MUX

The only change needed in the YAML files is that GPIO8 now enables **both**
MUX boards simultaneously rather than selecting between them. Remove the
GPIO9 MUX-B enable logic — both boards share GPIO8 on their EN pins:

```yaml
# Both MUX boards EN pins wired to GPIO8
# turn_off drives LOW → both MUX boards enabled at same time
- output.turn_off: mux_en   # enables both MUX-A and MUX-B

# Remove mux_b_en references entirely
# GPIO9 is now free for other use
```

The channel scanning lambda in each sensor stays exactly the same —
S0–S3 addressing is identical, and both MUX boards respond to the same
address simultaneously.
