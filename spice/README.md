# Analog Front-End SPICE Model

LTspice model of the probe-side analog path described in
[`wood_moisture_project.md`](../wood_moisture_project.md) and
[`wood_moisture_design_update.md`](../wood_moisture_design_update.md).

## What's modeled

The corrected **dual-MUX resistive divider** that reads wood resistance:

```
GPIO9 3.3V ─ R1(10k) ─ MUX-A(Ron) ─ ProbeA+ ─[ R_wood ]─ ProbeB+ ─ MUX-B(Ron) ─ SIG_B ─┬─ R2(100k) ─ GND
                                                                                        └─ GPIO1 ADC
```

`V_adc = VCC · R2 / (R1 + Ron_A + R_wood + Ron_B + R2)`

- `Ron` — CD74HC4067 on-resistance (~70 Ω @ 3.3 V), one per MUX, in series.
- `Rleak` — aggregate off-channel leakage on the sense node. In the dual-MUX
  design the excite and sense SIG pins are separate nodes, so leakage is
  common-mode; the default `1e12` (ideal) reproduces the design table.

## Files

| File | Purpose |
|---|---|
| `wood_afe.net` | Dual-MUX front end. Sweeps `R_wood` across the documented table. |
| `wood_afe_singlemux.net` | Old single-MUX topology, showing the leakage error the redesign fixes. |

## Running

Both are plain SPICE netlists — open in the LTspice GUI and Run, or headless:

```bash
LTspice -b wood_afe.net          # writes wood_afe.raw + wood_afe.log
# read the .meas result out of the (UTF-16) log:
tr -d '\000' < wood_afe.log | grep -A20 'Measurement: vadc'
```

On macOS use `-b` (never `-Run`) and pass a **netlist**, not a `.asc` — the Mac
LTspice build can't netlist a schematic from the command line. Same invocation
works under Linux/Wine and via the `mcltspice` MCP server.

## Validation

`wood_afe.net`, run on LTspice XVII 17.2.4, matches the expected-voltage table
in `wood_moisture_project.md` to within a few mV at every point:

| R_wood | Model V(adc) | Doc table | MC% |
|---|---|---|---|
| ~0 (short) | 2.996 | 3.00 | 28% clamp |
| 5 kΩ | 2.866 | 2.87 | >30% |
| 8 kΩ | 2.793 | 2.80 | 28% |
| 20 kΩ | 2.536 | 2.54 | 25% |
| 35 kΩ | 2.274 | 2.28 | 20% |
| 50 kΩ | 2.061 | 2.06 | 16% |
| 100 kΩ | 1.570 | 1.57 | 13% |
| 300 kΩ | 0.805 | 0.80 | 9% |
| 500 kΩ | 0.541 | 0.54 | 8% |
| 750 kΩ | 0.384 | 0.38 | 7% |
| 1.5 MΩ | 0.205 | 0.21 | 6% clamp |

The small deltas vs the table come from including the two `Ron` terms the
hand-calc omits.

## Why dual-MUX (the leakage study)

`wood_afe_singlemux.net` adds the aggregate off-channel leakage as a shunt that
bypasses `R_wood` — which is what a shared excite/sense SIG pin does. At the dry
end the error is large:

| R_wood | Dual-MUX (true) | Single-MUX leaky | Reads as |
|---|---|---|---|
| 100 kΩ | 1.570 V | 1.706 V | slightly wetter |
| 300 kΩ | 0.805 V | 1.109 V | ~11% (truly 9%) |
| 500 kΩ | 0.541 V | 0.916 V | ~12% (truly 8%) |
| 750 kΩ | 0.384 V | 0.805 V | 9% (truly 7%) |
| 1.5 MΩ | 0.205 V | 0.680 V | ~9% (truly 6%) |

Several % MC of error across the dry range — the concrete reason the design
separates the excite and sense SIG nodes.

## Not yet modeled

- **AC excitation dynamics.** The firmware toggles GPIO9 to avoid DC
  polarization of the wood/electrode interface. This model is the resistive
  DC divider only. Adding a wood double-layer capacitance (`C_wood` across
  `R_wood`) plus a `PULSE` source on `VEXC` would show the settling the AC
  drive relies on.
- **ESP32-S3 ADC nonlinearity** (±10% raw). The model reports the ideal node
  voltage; ADC calibration is handled in ESPHome.
