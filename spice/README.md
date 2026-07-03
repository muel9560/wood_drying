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
| `wood_afe.net` | Dual-MUX front end. Sweeps `R_wood` across the documented table. Authoritative CLI/headless model. |
| `wood_afe_sch.asc` | Same circuit as an LTspice schematic — open in the GUI to view/edit. |
| `wood_afe_singlemux.net` | Old single-MUX topology, showing the leakage error the redesign fixes. |
| `wood_afe_ac.net` | AC excitation + wood capacitance — shows the ADC settling time the firmware dwell must respect. |
| `wood_afe_r2sweep.net` | R2 value trade-off study behind the component-selection notes below. Analysis only — defaults unchanged. |

> Opening `wood_afe_sch.asc` and hitting Run makes LTspice write its own netlist
> next to it (`wood_afe_sch.net`, gitignored). The schematic and the curated
> `wood_afe.net` are deliberately named differently so a GUI run never overwrites
> the hand-maintained netlist. Both describe the identical circuit and give the
> same result.

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

## AC excitation and settling time (`wood_afe_ac.net`)

The firmware toggles GPIO9 (a unipolar 0↔3.3 V square wave) rather than holding
DC, to avoid polarizing the wood/electrode interface. That interface plus the
wood bulk act as a capacitance `C_wood` in parallel with `R_wood`, so `V(sigb)`
does not step instantly on each edge — it charges with

```
tau ≈ C_wood · (R_wood ∥ (R1 + R2))
```

At each rising edge an *uncharged* `C_wood` momentarily shorts `R_wood`, so
`V(sigb)` jumps toward the wet-clamp voltage (~2.5–3.0 V) and then decays to the
true divider value over ~5·tau. **Sample the ADC too soon and dry wood reads
soaking wet.** Measured (`Cw = 10 nF`), settled value vs a sample taken 200 µs
after the edge:

| R_wood | tau | True V(sigb), long dwell | Sampled 200 µs after edge | Apparent error |
|---|---|---|---|---|
| 8 kΩ (wet, ~28%) | ~75 µs | 2.793 V | 2.807 V | negligible |
| 100 kΩ (~13%) | ~520 µs | 1.570 V | 2.546 V | reads like ~25% MC |
| 1.5 MΩ (~6%) | ~1.0 ms | 0.205 V | 2.505 V | reads like ~25% MC |

The settled column matches `wood_afe.net` exactly — the models agree once the
node has settled.

**Firmware implication:** dwell at least ~5·tau after toggling excitation before
sampling. The required dwell grows as the wood dries; sizing for the dry case
(~5 ms at `Cw = 10 nF`, 1.5 MΩ) covers the whole range. `Cw` here is an estimate
— measure a known-dry channel's settling curve and tune `Cw` to match, then the
model gives you the dwell for every moisture level.

## Component value selection (`wood_afe_r2sweep.net`)

**The shipping values are `R1 = 10k`, `R2 = 100k`, and they stay that way.**
They are near-optimal for the full moisture range: `R2 = 100k` is almost exactly
the geometric mean of the wood-resistance span, `√(8k · 1.5M) ≈ 110k`, which is
the pull-down that best spreads 8 kΩ–1.5 MΩ across the ADC. Peak sensitivity
(`V ≈ VCC/2`) lands at `R_wood ≈ R1+R2 ≈ 110k`, i.e. ~13% MC. `R1 = 10k` only
matters at the wet end, where it sets the ~3.0 V short-detection clamp; it is
negligible against dry-wood resistances and needs no change.

Where 100k is weakest is the bone-dry end — exactly where you decide "is it
furniture-ready?" Sweeping the pull-down (`R1` fixed at 10k):

| R_wood (MC) | R2=100k | R2=220k | R2=390k |
|---|---|---|---|
| 100 kΩ (13%) | 1.570 | 2.199 | 2.573 |
| 300 kΩ (9%) | 0.805 | 1.369 | 1.838 |
| 500 kΩ (8%) | 0.541 | 0.994 | 1.430 |
| 750 kΩ (7%) | 0.384 | 0.741 | 1.119 |
| 1.5 MΩ (6%) | **0.205** | **0.420** | 0.677 |
| 6→13% span | 1.37 V | 1.78 V | 1.90 V |

At `R2 = 100k`, 6% MC reads **0.205 V** — down in the ESP32-S3 ADC's noisy,
nonlinear low-voltage floor (~0.1–0.15 V), so the driest readings sit where the
ADC is least trustworthy. `R2 = 220k` lifts that to **0.42 V** and widens the
6–13% band from 1.37 V to 1.78 V, while 13% still reads 2.2 V (not railed).
`R2 = 390k` compresses the wet end toward the rail for little extra gain, and
pushes the ADC source impedance too high.

### Optional dual-range (auto-ranging) for the final drying

Rather than a second dedicated system (duplicated hardware + re-clipping every
probe when a stack graduates), make R2 **switchable on the same board**:

- Keep `R2 = 100k` as the default broadband range.
- Add a second pull-down so R2 can become ~220k, selected per channel.
  Cheapest form: two pull-downs (100k + 220k), each to its own GPIO — drive the
  chosen one LOW (it acts as ground) and set the other to input / hi-Z. No extra
  chips, one spare GPIO. (An analog switch such as a 74HC4066 works too.)
- Firmware: read on the 100k range; when a channel drops to **≤ ~9% MC
  (V < ~0.8 V)**, re-read that channel on the 220k range and use that value for
  the dry-end MC. This is the same idea as the PSoC 5LP's PGA auto-ranging, done
  with one resistor.

Because R2 changes, the solve constant changes with it:

```
R_wood = (R2 · VCC / V) − R2 − R1      # use R2 = 220k on the dry range
```

Higher R2 also raises the ADC source impedance, so on the 220k range add a
~10–47 nF cap on the `sigb` node to feed the ADC sample-and-hold, and allow a
bit more settling dwell — the same `tau = C_wood·(R_wood ∥ (R1+R2))` trade-off
that `wood_afe_ac.net` models.

## Not yet modeled

- **ESP32-S3 ADC nonlinearity** (±10% raw). The model reports the ideal node
  voltage; ADC calibration is handled in ESPHome.
