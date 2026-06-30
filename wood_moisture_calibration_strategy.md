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

## ESPHome Calibration Adjustment Reference

Once you have cross-check data, adjust the logarithmic curve coefficients in
your YAML substitutions to match your measured values:

```yaml
substitutions:
  # Black walnut log-linear curve: MC% = cal_a - cal_b × log10(R_ohms)
  # Default values based on USDA FPL data with walnut correction:
  cal_a: "31.62"
  cal_b: "2.985"
  # After field calibration, adjust these to match your cross-check readings.
  # Example: if sensor reads 8.5% but 2019 pieces + handheld both read 7.2%,
  # reduce cal_a by ~1.3 to bring readings into alignment.
```

A two-point calibration (one point from 2019 EMC pieces, one from weight-method
mid-range) is sufficient to fit a corrected curve for your specific sensor
circuit and probe geometry.
