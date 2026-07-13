# Bench-validating the ADS1220 front end

The component's register values follow the datasheet but have **never been run against
a real chip**. Do this before trusting a reading, and certainly before laying out a PCB.

Config: [`../../ads1220_bench.yaml`](../../ads1220_bench.yaml). No mux, no probes, one
channel, 10 s interval, `logger: DEBUG`.

## Wiring

Breadboard the ADS1220 module and the XIAO. There is **no mux and no 74AHCT541** — AIN0
goes straight to the resistor under test.

```
XIAO 5V   ─── AVDD                 XIAO 3V3 ─── DVDD
XIAO GND  ─── AGND  and  DGND      <-- the module does NOT bond these internally
                                       (measured open between the AGND and DGND pins)
ADS1220 CLK ─── DGND               <-- selects the internal oscillator. NOT optional.
                                       The module has ~5k to DGND, which is not a tie.

XIAO GPIO7 ─ SCLK    GPIO8 ─ MISO(DOUT)    GPIO9 ─ MOSI(DIN)
XIAO GPIO1 ─ CS      GPIO2 ─ DRDY

           AIN0 ──[ R_dut ]── RET_BUS ──[ R_ref 220k ]── AGND
                                 │
                    AIN1 ────────┤
                    REFP0 ───────┘        REFN0 ─── AGND
```

Measure your actual R_ref with a good meter and put that number in the YAML's
`r_ref` substitution. It is the only calibration anchor — every reading scales
directly with it.

The design target is 200 kΩ; the numbers below assume the 220 kΩ actually on the bench.
R_ref sets the dry-end ceiling (full scale at gain 1 is exactly `R_dut = R_ref`), so
220 kΩ reads slightly *drier* wood than the design, not less accurately. If your R_ref
is not a 0.1% part, its tolerance lands on every reading — but only as a shift in R, and
MC is logarithmic in R, so a 5% R_ref error moves MC by ~0.16%. Measure it; don't trust
the colour bands.

## Step 1 — does it talk?

On boot the component writes its registers and reads them straight back. Look for:

```
[I][ads1220_moisture]: ADS1220 configured (REG1=0x00 REG2=0x73 REG3=0x20)
```

If instead you get `REGn readback 0xXX, expected 0xYY` and the component marks itself
failed, the SPI bus is wrong — check CS, check MISO isn't floating, and check **CLK is
tied to DGND** (a floating CLK means no conversions at all). This check exists so a
wiring fault looks like a wiring fault, not like plausible noise.

`REG2 = 0x73` = external reference + 60 Hz rejection + 10 µA IDAC. If you set
`line_frequency: 50Hz` it reads `0x72`.

## Step 2 — does it measure?

Swap R_dut and compare **Bench R** to the resistor's real value. With R_ref = 220 kΩ:

| R_dut | expected R | expected gain (DEBUG log) | expected MC |
|---|---|---|---|
| 10 kΩ | 10 kΩ | **16** | 15.7% |
| 22 kΩ | 22 kΩ | 4 | 13.1% |
| 100 kΩ | 100 kΩ | 1 | 8.15% |
| ~200 kΩ (optional, see below) | ~200 kΩ | 1 | 6.0% (floor) |
| 1 MΩ | **NAN** | rails at gain 1 | NAN |
| open | **NAN** | rails at gain 1 | NAN |

The gain column is *not* a constant of the circuit — it is a function of `R_dut / R_ref`,
so it moves when R_ref does. At the design's 200 kΩ, 10 kΩ lands on gain 8; at 220 kΩ the
ratio is smaller, one more doubling fits under the 0.75-of-full-scale target, and the
autoranger picks **16**. That is correct behaviour, not a bug. (Likewise 22 kΩ / 220 kΩ
is exactly 0.1, which is why it takes gain 4 — the same gain the old 20 kΩ / 200 kΩ row
took, for the same reason.)

The optional row replaces the old 150 kΩ one: anything from roughly 120 kΩ to 215 kΩ
exercises the top of the range at gain 1. Build it from what you have — two 100 kΩ in
series, or 100 kΩ + 22 kΩ. Which you pick changes the expected MC, so check against the
right one:

| series R_dut | expected R | expected MC |
|---|---|---|
| 100k + 22k = 122 kΩ | 122 kΩ | 7.5% |
| 100k + 100k = 200 kΩ | 200 kΩ | **6.0%** — computed 5.9%, clamped by the dry floor |

The 200 kΩ case is the more interesting of the two precisely *because* it clamps: it shows
that near the ceiling **Bench R** still reports the true resistance while **Bench MC**
flattens onto its 6% floor. R is the number that validates the front end. MC, once it
clamps, cannot.

The 1 MΩ and open cases are *supposed* to fail: full scale at gain 1 is exactly
`R_dut = R_ref`, so anything above 220 kΩ is out of range by design. That is the dry-end
ceiling (~5.6% MC at 220 kΩ, ~5.9% at the design's 200 kΩ — both under the 6% floor), and
it is why R_ref is large and AVDD is 5 V — see `docs/analog-front-end-design.md` in the
wood-sensor-ads repo.

Compare against each resistor's **measured** value, not its printed one. With 1% or 5%
parts the nominal is the wrong yardstick — a 5% 22 kΩ can legitimately be 21 kΩ, and the
ADC is not wrong for saying so. Judge the ratio, not the label. If the reading is out by a
**factor of two**, `r_ref` is wrong. If it is out by a factor of 2/4/8/16, the autorange
gain is being applied twice or not at all — check the `gain=` in the DEBUG line against
the table above.

## Step 3 — the things most likely to be wrong

Because none of this has run on hardware, these are my honest suspects, in order:

1. **SPI mode.** The component uses CPOL=0 / CPHA=1 (mode 1), which is what the ADS1220
   wants. If step 1 passes, this is fine — a wrong mode would corrupt the readback.
2. **Sign extension / byte order** in `read_conversion_()`. A 100 kΩ resistor reading
   *negative* or wildly wrong points here.
3. **Autorange.** Verify the `gain=` column above. The ratio is divided by the gain, so
   a gain bug shows up as a clean power-of-two error.
4. **Conversion timing.** If DRDY never asserts you'll see `CHx: DRDY never asserted`.
   That is the DRDY pin, not the maths.

## Step 4 — only then

Once a known resistor reads back correctly across the range, add the mux and the
74AHCT541 and re-check one channel through the mux. The mux's ~100 Ω on-resistance adds
to every reading; because MC is logarithmic in R, a 2% error in R is only ~0.07% MC, so
it is ignorable — but it should be *visible* as a small positive offset at the 10 kΩ end,
and if it isn't, the mux isn't actually in circuit.
