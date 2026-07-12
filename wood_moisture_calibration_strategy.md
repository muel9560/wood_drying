# Wood Moisture Sensor Calibration Strategy
## Using Multiple Sample Types, Handheld Meter, Probe Sensors, and Oven-Dry Weight Method

---

## Question

Is there a good way to use multiple smaller pieces of walnut from the same tree
that are slightly different sizes cut at different times to take readings from
the handheld meter, the probe/sensors and weight to cross check and calibrate
the sensors? I have some pieces that were cut in 2019 that should be at EMC
based on how long ago it was cut, I have some pieces that were cut last year on
one face and fresh on the other. I would like to use the oven and weighing
method to check starting and ending MC on the fresher pieces to see where they
are starting out and how much moisture is actually in them to help calibrate
since the accuracy seems to diminish above 30%. I don't want to calibrate to
the fresh cut not knowing where that MC is at to start. I also don't want to
use the oven dried method to determine end point either since that may be below
the EMC currently.

---

## Why Your Instincts Are Correct

Pin meters lose accuracy above ~28-30% MC because resistance becomes very low
and differences in resistance compress into a tiny voltage range — small errors
in the reading circuit produce large MC% errors. Oven-dry weight gives you true
0% MC but that's below your actual target range and below current EMC, so it's
a reference point rather than a working calibration point. Your three sample
categories give you anchor points at known positions across the useful range
without either of those problems.

---

## The Three Sample Categories and What Each Gives You

**2019 pieces (at EMC)** — these are your most valuable calibration anchor.
Wood that has been air drying since 2019 in your climate has reached equilibrium
moisture content. You don't need to know the exact MC to use these — the meter,
the sensors, and the weight method should all agree on whatever EMC currently
is. Any disagreement between your handheld meter and your sensor on these pieces
tells you your sensor offset directly. Boise, Idaho EMC in a garage typically
runs 6–9% depending on season, so you have a reliable low-end anchor in a range
where pin meters are most accurate.

**Pieces cut last year on one face, fresh on the other** — these give you a
mid-range point somewhere between green and EMC, probably in the 15–25% range
depending on species, thickness, and storage. The face cut last year has been
drying for a year so will be closer to shell equilibrium. The fresh-cut face is
still green. This gives you a natural moisture gradient sample to work with.

**Fresh-cut pieces for oven-dry method** — your plan here is exactly right. Use
oven-dry to establish the bone-dry weight, which gives you a denominator for
calculating MC% from weight at any future point. You don't need to know starting
MC to do this — you just need starting wet weight and ending bone-dry weight.

---

## The Oven-Dry Calibration Procedure for Fresh Pieces

Cut small samples (called moisture sections or cookie cuts) from the fresh
pieces — ideally 1–2 inches thick, cut from at least 12 inches in from the end
to avoid end grain drying effects.

### Step 1 — Take All Readings Immediately at Cut

- Weigh each sample on a kitchen scale, record as **W_wet**
- Take handheld pin meter reading at the cut face center — record
- Insert your probe sensors into the sample if possible, or note that the cut
  face reading is your starting reference
- Mark and number each sample

### Step 2 — Oven Dry the Samples

- Dry at 215–220°F (not higher — above 220°F you start driving off bound water
  and changing the wood chemistry)
- Weigh every 2–3 hours
- Stop when weight stabilizes across two consecutive weighings — this is
  **W_dry** (bone dry weight)

### Step 3 — Calculate True Starting MC%

```
MC% = ((W_wet - W_dry) / W_dry) × 100
```

This gives you a true MC% at the time of cutting that you can use as a
calibration reference for both your handheld meter and your sensors at the high
end of the range. Importantly you're only using it to understand where the fresh
wood started — not to calibrate the endpoints of the sensor, which would be
unreliable at that MC level.

### Step 4 — Track Weight Through Drying

Weigh the same samples periodically as they dry alongside your main stack. At
any point during drying you can calculate current MC% from weight:

```
Current MC% = ((W_current - W_dry) / W_dry) × 100
```

This gives you ground truth MC% at whatever point in the drying curve you're
at, which you can cross-check against both the handheld meter and your sensors.
This is where the weight method becomes very useful — it's accurate at any MC%,
including the 15–25% range where pin meters start getting noisy.

---

## Cross-Check and Calibration Matrix

Using all three sample categories together gives you this calibration matrix:

| Sample type | MC% range | Weight method | Handheld meter | Sensor | Purpose |
|---|---|---|---|---|---|
| 2019 pieces | ~6–9% (EMC) | Use as reference | Most accurate here | Calibrate sensor offset | Low-end anchor |
| Last year / fresh face | ~15–25% | Track over time | Moderate accuracy | Cross-check | Mid-range validation |
| Fresh cut samples | 30–80% at cut | Oven-dry gives true MC | Unreliable above 30% | Unreliable above 30% | Starting point only, track weight curve down |
| Fresh cut at ~20% | ~20% (during drying) | Most reliable here | Good accuracy | Calibrate against weight | Mid-range anchor |
| Fresh cut at ~10% | ~10% (late drying) | Reliable | Good accuracy | Validate sensor | Upper working range |

The key insight is that the weight method is your most reliable reference across
the full range, the handheld meter is most reliable below 25%, and your sensors
need to be calibrated in the range where they'll actually be used (8–25%) rather
than at the extremes.

---

## Practical Recommendation for Sensor Calibration

**Phase 1 — immediately:** Take readings from the 2019 pieces with both the
handheld meter and your sensors. These pieces are at EMC and stable. Any offset
between the handheld and your sensor at this low end is your baseline correction
factor. Apply it as an offset in the ESPHome lambda.

**Phase 2 — within the first few weeks:** As your fresh-cut moisture sections
dry through the 20–25% range, you'll have weight-derived true MC% values to
compare against both the handheld and sensors at mid-range. Adjust your `cal_a`
and `cal_b` logarithmic curve coefficients at this point.

**Phase 3 — ongoing:** The 2019 pieces remain as a permanent reference. Any
time you want to verify sensor drift, re-read those pieces. They are
self-referencing because they will always be at whatever the current EMC is in
your garage, and that's exactly where your drying target is.

---

## One Additional Suggestion

If you have enough small offcuts from the 2019 material, keep several in a
sealed plastic bag and several exposed to the garage air simultaneously. The
sealed ones will stay at whatever MC they were when sealed (a snapshot
reference), while the exposed ones track current EMC. Weighing both periodically
gives you a running picture of seasonal EMC variation in your specific garage
without needing any sensors at all — useful context for interpreting your stack
readings across seasons.

---

## Oven-Dry Calibration Run — Black Walnut 2in Deep Piece (2026-07)

First real calibration run against weight, using the procedure above on the
`CH-A01` "2in Cal Deep" piece. It replaces the generic USDA coefficients with a
curve fit to this wood on this circuit. **Status: provisional** — the piece was
still losing weight when the fit was made, so the intercept is not yet final
(see "Oven-dry weight" below). The slope is already trustworthy.

### Data sources

- **`logs/weight.csv`** — the piece weighed on a kitchen scale roughly every
  2–3 h from 2026-07-03 (1883 g, start of oven drying) onward.
- **`logs/woodstack01-mer-logs-26-*.txt`** — the ESPHome DEBUG logs. Each
  `CH-A01 r=<ohms>` line is one raw (pre-clamp) resistance reading; the piece was
  clipped to the sensor for several polls each time it came out of the oven.

### Method

1. **Extract** every `CH-A01 r=` reading and stamp it with the date from the log
   filename + the `[HH:MM:SS]` line time.
2. **Cluster** readings into weigh sessions (gaps > 25 min start a new session)
   and take the **median R per session** — the dry-end readings are noisy, so the
   median is more robust than the mean.
3. **Match** each session to the nearest `weight.csv` entry (all within a few
   minutes) and compute true MC from weight:
   `MC% = (W − W_dry) / W_dry × 100`.
4. **Fit** `MC = a − b·log10(R)` by least squares over the sessions where R is
   still on-scale (R < ~300 kΩ). Readings above ~1 MΩ sit in the ESP32-S3 ADC
   noise floor and are excluded from the fit.

### Oven-dry weight (W_dry)

The piece had **not** reached constant weight at fit time — over the last day it
was still dropping ~1.2–1.4 g/h (3–5 g per weigh) with no plateau. Fitting an
exponential decay `W(t) = W_dry + A·e^(−t/τ)` to the full weight series:

```
W_dry ≈ 1481 g    (R² = 0.9948, τ ≈ 49 h)
```

At the last weigh (1548 g) that is ~67 g / ~4.5% MC still to lose, and with a
~49 h time constant it is a couple more days from constant weight. **W_dry = 1481 g
is therefore an extrapolation, not a measurement.** Re-run this fit once the
piece holds constant weight to lock W_dry and the curve intercept.

### Session data (W_dry = 1481 g)

| Session (2026) | polls | median R (Ω) | W (g) | true MC% | in fit |
|---|---:|---:|---:|---:|:--:|
| 07-04 20:53 | 5 | 27,694 | 1672 | 12.90 | ✓ |
| 07-05 00:06 | 5 | 34,443 | 1660 | 12.09 | ✓ |
| 07-05 07:12 | 4 | 46,560 | 1635 | 10.40 | ✓ |
| 07-05 09:45 | 6 | 52,431 | 1627 | 9.86 | ✓ |
| 07-05 11:55 | 5 | 57,270 | 1621 | 9.45 | ✓ |
| 07-05 14:12 | 8 | 68,834 | 1617 | 9.18 | ✓ |
| 07-05 16:32 | 8 | 79,875 | 1609 | 8.64 | ✓ |
| 07-05 18:49 | 9 | 82,801 | 1606 | 8.44 | ✓ |
| 07-05 22:17 | 10 | 112,704 | 1595 | 7.70 | ✓ |
| 07-06 00:18 | 9 | 160,844 | 1590 | 7.36 | ✓ |
| 07-06 06:54 | 14 | 1,165,260 | 1571 | 6.08 | — noise floor |
| 07-06 09:21 | 12 | 3,450,983 | 1568 | 5.87 | — noise floor |
| 07-06 11:39 | 7 | 6,233,788 | 1565 | 5.67 | — noise floor |
| 07-06 13:54 | 11 | 12,062,381 | 1561 | 5.40 | — noise floor |
| 07-06 16:25 | 5 | 13,422,220 | 1558 | 5.20 | — noise floor |

### Fitted curve

```
MC% = 45.8 − 7.53 × log10(R_ohms)      (n = 10, R² = 0.94, valid ~7–13% MC)
```

The slope is well-determined and **insensitive to the extrapolated W_dry** — at
W_dry of 1470 / 1481 / 1490 g it comes out 7.59 / 7.53 / 7.49 with identical R².
Only the intercept moves with the final dry weight (roughly +1 per −10 g of
W_dry), which is what the constant-weight re-run will pin down.

### What this shows

- **The shipped USDA curve (`31.62 − 2.985·log10 R`) over-reads this walnut by
  5–9 points** across the whole range. This is the source of every earlier
  handheld-vs-sensor disagreement.

  | R (Ω) | true MC (weight) | shipped 31.62/2.985 | new fit 45.8/7.53 |
  |---:|---:|---:|---:|
  | 27,694 | 12.9% | 18.4% | 12.3% |
  | 79,875 | 8.6% | 17.0% | 8.9% |
  | 160,844 | 7.4% | 16.1% | 6.6% |

- **This wood's curve is much steeper** (7.5 vs 3.0 %/decade). Resistance climbs
  far faster as it dries than the generic curve assumes.

- **Below ~7% MC (R > ~1 MΩ) the reading is not usable.** True MC there is
  5–6%, but the median R jumps from 1 MΩ to 13 MΩ across barely 1% of MC and the
  fit extrapolates to negative MC — the readings are in the ADC noise floor. This
  matches the hardware ceiling (~24 MΩ ≈ where V hits the 0.03 V open-circuit
  guard on the 220 kΩ range) and the observed open/short on the fully-dried
  piece. **Trust the sensor only in ~7–14% MC.** No pin meter does better at the
  dry end; this is a limit of resistance sensing, not this build.

### Reproduce

The extraction/clustering/fit was done in Python (numpy) directly against
`logs/weight.csv` and the `logs/woodstack01-mer-logs-26-*.txt` files, using the
method steps above. Re-run it after the piece reaches constant weight, then
update `cal_a`/`cal_b` with the final intercept and slope.

---

## ESPHome Calibration Adjustment Reference

Adjust the logarithmic curve coefficients in your YAML substitutions to match
your measured values:

```yaml
substitutions:
  # Black walnut log-linear curve: MC% = cal_a - cal_b × log10(R_ohms)
  # Original generic values (USDA FPL): cal_a 31.62 / cal_b 2.985 — these
  # over-read THIS walnut by 5-9 points (see "Oven-Dry Calibration Run" above).
  #
  # Provisional fit to the 2in Cal Deep oven-dry run (valid ~7-14% MC):
  cal_a: "45.8"
  cal_b: "7.53"
  # Provisional: intercept (cal_a) refines once the piece holds constant weight.
  # Slope (cal_b) is already stable across the W_dry uncertainty.
```

A curve fit against weight across the working range (as in the run above) is the
reliable calibration. The 2019 EMC pieces remain a good permanent low-end
cross-check for drift.
