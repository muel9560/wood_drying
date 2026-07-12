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

           AIN0 ──[ R_dut ]── RET_BUS ──[ R_ref 200k 0.1% ]── AGND
                                 │
                    AIN1 ────────┤
                    REFP0 ───────┘        REFN0 ─── AGND
```

Measure your actual R_ref with a good meter and put that number in the YAML's
`r_ref` substitution. It is the only calibration anchor — every reading scales
directly with it.

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

Swap R_dut and compare **Bench R** to the resistor's real value. With R_ref = 200 kΩ:

| R_dut | expected R | expected gain (DEBUG log) | expected MC |
|---|---|---|---|
| 10 kΩ | 10 kΩ | 8 | 15.7% |
| 20 kΩ | 20 kΩ | 4 | 13.4% |
| 100 kΩ | 100 kΩ | 1 | 8.2% |
| 150 kΩ | 150 kΩ | 1 | 6.8% |
| 1 MΩ | **NAN** | rails at gain 1 | NAN |
| open | **NAN** | rails at gain 1 | NAN |

The 1 MΩ and open cases are *supposed* to fail: full scale at gain 1 is exactly
`R_dut = R_ref`, so anything above 200 kΩ is out of range by design. That is the dry-end
ceiling (~5.9% MC), and it is why R_ref is 200 kΩ and AVDD is 5 V — see
`docs/analog-front-end-design.md` in the wood-sensor-ads repo.

With 0.1% parts the reading should land within a few tenths of a percent. If it is out
by a **factor of two**, `r_ref` is wrong. If it is out by a factor of 2/4/8/16, the
autorange gain is being applied twice or not at all — check the `gain=` in the DEBUG
line against the table.

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
