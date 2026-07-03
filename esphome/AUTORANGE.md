# Dual-range (auto-ranging) moisture read

Optional add-on for the final-drying endgame. Reads each channel on the default
100k pull-down; when a channel is dry enough that its reading sinks into the
ESP32-S3 ADC's noisy low-voltage floor, it re-reads on a 220k pull-down where
the same wood resistance produces a larger, cleaner voltage. See
[`../spice/README.md`](../spice/README.md) → "Component value selection" for the
numbers and [`../spice/wood_afe_autorange_sch.asc`](../spice/wood_afe_autorange_sch.asc)
for the wiring.

## Fix the resistance formula first (applies to every channel)

The current channel lambdas compute resistance as:

```cpp
float r = (${r1_value} * 3.3f / v) - ${r1_value} - ${r2_value};   // WRONG
```

The divider is `V = VCC · R2 / (R1 + R_wood + R2)`, so the solve is:

```cpp
float r = (${r2_value} * 3.3f / v) - ${r1_value} - ${r2_value};   // R2 in the numerator
```

With the old line a real reading (v = 1.57 V at 100 kΩ) yields a negative
resistance and the `r <= 0` guard returns 28% — the sensor reads ~28% for
almost everything. Fix this on the main channels regardless of auto-ranging.

## Hardware

Two pull-downs from the ADC node (`sigb`, GPIO1), each grounded by its own GPIO
used as a switched ground — no extra chips:

| Resistor | From → to | Grounded by |
|---|---|---|
| R2_lo = 100 kΩ | `sigb` → GPIO10 | GPIO10 |
| R2_hi = 220 kΩ | `sigb` → GPIO11 | GPIO11 |
| Cadc = 22 nF (optional) | `sigb` → GND | — (feeds ADC S/H on the 220k range) |

- Select a range by driving that resistor's GPIO **LOW** (it becomes ground) and
  setting the other GPIO to **INPUT / hi-Z** (that resistor is disconnected;
  input leakage is ~nA against 100k+).
- GPIO10/11 are otherwise-unused ESP32-S3 pins; adjust if your board differs.

## Config additions

```yaml
substitutions:
  # ... existing ...
  r2_lo: "100000.0"          # default broadband range
  r2_hi: "220000.0"          # dry-end range
  autorange_v_thresh: "0.80" # on the 100k range, below this re-read on 220k
                             # (0.80 V ~ R_wood 300k ~ 9% MC)
  pin_r2_lo: "10"            # GPIO grounding the 100k pull-down
  pin_r2_hi: "11"            # GPIO grounding the 220k pull-down
  hi_extra_dwell: "200"      # extra ms settle on the 220k range (higher R -> slower)

esphome:
  on_boot:
    priority: 600
    then:
      - output.turn_off: excite_pin
      - output.turn_off: mux_en
      - lambda: |-
          // default to the 100k range at boot
          pinMode(${pin_r2_hi}, INPUT);
          pinMode(${pin_r2_lo}, OUTPUT); digitalWrite(${pin_r2_lo}, LOW);
```

## Auto-range channel lambda (example: CH-A00)

Drop-in replacement for a channel's `lambda:`. The only per-channel differences
are the MUX address lines (`mux_s0..s3`) and the log tag.

```yaml
  - platform: template
    name: "2in Calibbration Shallow"
    id: m_a00
    unit_of_measurement: "%"
    accuracy_decimals: 1
    icon: "mdi:water-percent"
    update_interval: ${scan_interval}
    lambda: |-
      // --- range-select helpers (raw GPIO switched grounds) ---
      auto range_lo = []() { pinMode(${pin_r2_hi}, INPUT);
                             pinMode(${pin_r2_lo}, OUTPUT); digitalWrite(${pin_r2_lo}, LOW); };
      auto range_hi = []() { pinMode(${pin_r2_lo}, INPUT);
                             pinMode(${pin_r2_hi}, OUTPUT); digitalWrite(${pin_r2_hi}, LOW); };

      // --- select this channel on the MUX (address 0) ---
      id(mux_en).turn_on(); delay(${bleed_delay}); id(mux_en).turn_off();
      id(mux_s0).turn_off(); id(mux_s1).turn_off(); id(mux_s2).turn_off(); id(mux_s3).turn_off();
      delay(${addr_delay});

      // --- read on the default 100k range ---
      range_lo();
      id(excite_pin).turn_on();  delay(${read_delay});
      float v  = id(raw_adc).sample();
      id(excite_pin).turn_off();
      float r2 = ${r2_lo};

      // --- dry? re-read on the 220k range (lifts V off the ADC floor) ---
      if (!isnan(v) && v < ${autorange_v_thresh}) {
        range_hi();
        id(excite_pin).turn_on();  delay(${read_delay} + ${hi_extra_dwell});
        v  = id(raw_adc).sample();
        id(excite_pin).turn_off();
        r2 = ${r2_hi};
        ESP_LOGD("moisture", "CH-A00 autorange -> 220k v=%.4f", v);
      }
      range_lo();  // leave in the default state

      ESP_LOGD("voltage", "CH-A00 v=%.4f r2=%.0f", v, r2);
      if (v >= 3.26f || v <= 0.03f) {
        ESP_LOGW("moisture", "CH-A00 open/short v=%.4f", v);
        return NAN;
      }

      // correct solve, using whichever R2 was active:
      float r = (r2 * ${vcc} / v) - ${r1_value} - r2;
      if (r <= 0.0f)            return 28.0f;   // effectively short
      if (r > ${r_dry_clamp})   return 6.0f;    // below measurement range
      if (r < ${r_wet_clamp})   return 28.0f;   // above fiber saturation
      float mc = ${cal_a} - ${cal_b} * log10f(r);
      float t = id(stack_temp_c).state;
      if (!isnan(t)) mc -= ((t - 21.1f) / 5.6f) * 0.5f;
      else ESP_LOGW("moisture", "CH-A00 temp unavailable, skipping correction");
      return mc;
    state_class: measurement
```

## Why the 0.80 V threshold

From the simulations (`spice/wood_afe.net`, `spice/wood_afe_r2sweep.net`) on the
100k range:

| V(sigb) | R_wood | MC% | action |
|---|---|---|---|
| ≥ 0.80 V | ≤ 300 kΩ | ≥ ~9% | 100k reading is fine (mid ADC range) |
| < 0.80 V | > 300 kΩ | < ~9% | re-read on 220k (0.205 V → 0.42 V at 6%) |

Below ~9% MC the 100k reading has slid toward the ADC's noisy floor; the 220k
re-read puts it back in the linear zone. Above that, the extra range and dwell
buy nothing, so most channels most of the time take the single fast 100k read.

## Notes

- The 220k range's higher source impedance needs the `Cadc` cap and the extra
  dwell (`hi_extra_dwell`) — the `tau = C_wood·(R_wood ∥ (R1+R2))` trade-off from
  `spice/wood_afe_ac.net`. Tune `hi_extra_dwell` to your measured settling.
- This keeps the shipping design at 10k/100k; the 220k path is only ever used
  for channels that have already dried past ~9%.
- Calibrate each range independently — a channel read on 220k and the same
  channel read on 100k should agree at the handoff; if they don't, trim
  `cal_a`/`cal_b` or the resistor values, not the threshold.
