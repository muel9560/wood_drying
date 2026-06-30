# Perfboard Assembly Guide
## Wood Moisture Monitor Node — Soldered Prototype

---

## Board Selection

Use the **7×9cm** double-sided perfboard from your Rindion kit.
Grid: 27 rows (A through +) × 35 columns (1 through 35) at 2.54mm pitch.
Orient with the long side (9cm / 35 columns) horizontal and the short
side (7cm / 27 rows) vertical.

---

## Component Placement — Top View (Component Side)

All coordinates are (column, row) where column 1 is left edge and row A
is top edge. Components are inserted from the top side and soldered on
the bottom.

### XIAO ESP32-S3 — Cols 2-8, Rows B-H

USB-C connector faces the top edge (row A) for case access.

| Row | Col 2 (left pins) | Col 8 (right pins) |
|-----|-------------------|---------------------|
| B   | 3V3               | GND                 |
| C   | GPIO1 (ADC)       | GPIO2 (unused)      |
| D   | GPIO9 (Excite)    | GPIO3 (unused)      |
| E   | GPIO4 (S0)        | GPIO7 (S3)          |
| F   | GPIO5 (S1)        | GPIO8 (EN)          |
| G   | GPIO6 (S2)        | GPIO44 (unused)     |
| H   | GPIO43 (unused)   | GPIO45 (unused)     |

### R1 (1MΩ) — Cols 11-13, Row C

Horizontal placement.

| Pin | Position | Connects to |
|-----|----------|-------------|
| Leg 1 (GPIO9 side) | Col 11, Row C | Wire to XIAO GPIO9 (col 2, row D) |
| Leg 2 (JCT side)   | Col 13, Row C | Junction node (col 13, row D via short jumper) |

### R2 (10kΩ) — Cols 11-13, Row E

Horizontal placement.

| Pin | Position | Connects to |
|-----|----------|-------------|
| Leg 1 (JCT side) | Col 11, Row E | Junction node (col 13, row D via wire) |
| Leg 2 (GND side) | Col 13, Row E | GND bus |

### C1 (100nF) — Cols 16-18, Row C

Decoupling cap for MUX-A. Horizontal placement.

| Pin | Position | Connects to |
|-----|----------|-------------|
| Leg 1 (VCC) | Col 16, Row C | 3.3V bus |
| Leg 2 (GND) | Col 18, Row C | GND bus |

### C2 (100nF) — Cols 16-18, Row E

Decoupling cap for MUX-B. Horizontal placement.

| Pin | Position | Connects to |
|-----|----------|-------------|
| Leg 1 (VCC) | Col 16, Row E | 3.3V bus |
| Leg 2 (GND) | Col 18, Row E | GND bus |

### Junction Node — Col 13, Row D

This is the most critical point in the circuit. Five connections meet
at this single hole (or use adjacent holes in the same column and bridge
them with solder):

1. R1 bottom leg (from col 13, row C — bridge down to row D)
2. R2 top leg (from col 11, row E — wire to col 13, row D)
3. GPIO1 wire (from XIAO col 2, row C)
4. MUX-A SIG wire (from MUX-A SIG pin)
5. MUX-B SIG wire (from MUX-B SIG pin)

**Tip:** Use hole col 13 row D as the primary junction. Solder a short
bare wire stub vertically from the bottom of the board up through this
hole, then solder all five connections to it on the bottom side. This
gives you a strong mechanical joint and guarantees all five wires share
a single electrical node.

### MUX-A (Excitation) — Cols 7-14, Rows I-P

Orient with control pins (SIG, EN, S0-S3, VCC, GND) on the left (col 7)
and channel pins (C0-C15) on the right (col 14).

| Row | Col 7 (control) | Col 14 (channels) |
|-----|-----------------|---------------------|
| I   | SIG             | C0                  |
| J   | EN              | C1                  |
| K   | S0              | C2                  |
| L   | S1              | C3                  |
| M   | S2              | C4                  |
| N   | S3              | C5                  |
| O   | VCC             | C6                  |
| P   | GND             | C7                  |

Note: C8-C15 continue on additional pins if your breakout board has them.
If the DEVMO board has 16 channel pins along one edge, orient accordingly
and adjust column assignments.

### MUX-B (ADC Sense) — Cols 21-28, Rows I-P

Mirror image of MUX-A. Channel pins on left (col 21), control pins on
right (col 28).

| Row | Col 21 (channels) | Col 28 (control) |
|-----|--------------------|-------------------|
| I   | C0                 | SIG               |
| J   | C1                 | EN                |
| K   | C2                 | S0                |
| L   | C3                 | S1                |
| M   | C4                 | S2                |
| N   | C5                 | S3                |
| O   | C6                 | VCC               |
| P   | C7                 | GND               |

### Screw Terminals — Edges

**Left side (cols 1-2)** — MUX-A and MUX-B channels 0-5:

| Rows  | Terminal | Channel | Probe |
|-------|----------|---------|-------|
| S     | 2-pin    | CH0     | Shallow (MUX-A C0 + MUX-B C0) |
| T     | 2-pin    | CH1     | Deep   (MUX-A C1 + MUX-B C1) |
| (U)   | gap      | —       | — |
| V     | 2-pin    | CH2     | Shallow |
| W     | 2-pin    | CH3     | Deep |
| (X)   | gap      | —       | — |
| Y     | 2-pin    | CH4     | Shallow |
| Z     | 2-pin    | CH5     | Deep |

**Right side (cols 34-35)** — MUX-A and MUX-B channels 6-11:

| Rows  | Terminal | Channel | Probe |
|-------|----------|---------|-------|
| S     | 2-pin    | CH6     | Shallow |
| T     | 2-pin    | CH7     | Deep |
| (U)   | gap      | —       | — |
| V     | 2-pin    | CH8     | Shallow |
| W     | 2-pin    | CH9     | Deep |
| (X)   | gap      | —       | — |
| Y     | 2-pin    | CH10    | Shallow |
| Z     | 2-pin    | CH11    | Deep |

Each 2-pin terminal has one pin for the probe+ (excite/sense) wire and
one pin for the probe- (GND) wire.

---

## Bottom Side Wiring — Solder Connections

All wiring runs on the bottom (solder) side using 22 AWG solid core
wire or bare wire bridges between pads. Use the wire color coding
below for traceability.

### Power Bus

| From | To | Color | Notes |
|------|----|-------|-------|
| XIAO 3V3 (col 2, row B) | MUX-A VCC (col 7, row O) | Red | Main power |
| MUX-A VCC (col 7, row O) | MUX-B VCC (col 28, row O) | Red | Daisy chain |
| MUX-A VCC (col 7, row O) | C1 VCC (col 16, row C) | Red | Decoupling |
| MUX-B VCC (col 28, row O) | C2 VCC (col 16, row E) | Red | Decoupling |

### Ground Bus

| From | To | Color | Notes |
|------|----|-------|-------|
| XIAO GND (col 8, row B) | MUX-A GND (col 7, row P) | Black | Main ground |
| MUX-A GND (col 7, row P) | MUX-B GND (col 28, row P) | Black | Daisy chain |
| MUX-A GND (col 7, row P) | R2 bottom (col 13, row E) | Black | Pull-down |
| MUX-A GND (col 7, row P) | C1 GND (col 18, row C) | Black | Decoupling |
| MUX-A GND (col 7, row P) | C2 GND (col 18, row E) | Black | Decoupling |
| GND bus | All terminal - pins | Black | Probe returns |

### Signal Wires

| Signal | From | To | Color |
|--------|------|----|-------|
| GPIO1 (ADC) | XIAO col 2, row C | Junction col 13, row D | Teal/Cyan |
| GPIO9 (Excite) | XIAO col 2, row D | R1 top col 11, row C | Gold/Yellow |
| R1 bottom | Col 13, row C | Junction col 13, row D | (bridge) |
| R2 top | Col 11, row E | Junction col 13, row D | (wire) |
| MUX-A SIG | Col 7, row I | Junction col 13, row D | Blue |
| MUX-B SIG | Col 28, row I | Junction col 13, row D | Green |

### Address Bus (shared S0-S3)

| Signal | From | To (MUX-A) | To (MUX-B) | Color |
|--------|------|------------|------------|-------|
| S0 | XIAO col 2, row E | Col 7, row K | Col 28, row K | Orange |
| S1 | XIAO col 2, row F | Col 7, row L | Col 28, row L | Green |
| S2 | XIAO col 2, row G | Col 7, row M | Col 28, row M | Purple |
| S3 | XIAO col 8, row E | Col 7, row N | Col 28, row N | Blue |

### Enable (shared EN)

| Signal | From | To (MUX-A) | To (MUX-B) | Color |
|--------|------|------------|------------|-------|
| EN | XIAO col 8, row F | Col 7, row J | Col 28, row J | Pink |

### Channel Wires to Terminals

Each probe channel connects to the SAME channel number on BOTH MUX boards.
One terminal pin carries the MUX-A (excite) channel, the other carries the
MUX-B (ADC sense) channel.

| Channel | MUX-A pin | MUX-B pin | Terminal | Side |
|---------|-----------|-----------|----------|------|
| CH0 | Col 14, row I | Col 21, row I | Cols 1-2, row S | Left |
| CH1 | Col 14, row J | Col 21, row J | Cols 1-2, row T | Left |
| CH2 | Col 14, row K | Col 21, row K | Cols 1-2, row V | Left |
| CH3 | Col 14, row L | Col 21, row L | Cols 1-2, row W | Left |
| CH4 | Col 14, row M | Col 21, row M | Cols 1-2, row Y | Left |
| CH5 | Col 14, row N | Col 21, row N | Cols 1-2, row Z | Left |
| CH6 | Col 14, row O | Col 21, row O | Cols 34-35, row S | Right |
| CH7 | Col 14, row P | Col 21, row P | Cols 34-35, row T | Right |
| ... | continue for remaining channels | | | |

---

## Assembly Order

1. **Insert and solder the XIAO pin headers first** — if your XIAO doesn't
   have headers pre-soldered, solder them now so the XIAO sits flush with
   the perfboard.

2. **Solder the passive components** — R1, R2, C1, C2. Insert from the
   top, solder on the bottom. Trim leads after soldering.

3. **Build the junction node** — solder a bare wire through col 13 row D,
   then connect R1 bottom, R2 top, GPIO1, MUX-A SIG, and MUX-B SIG wires
   to this point on the bottom side.

4. **Solder the MUX breakout board headers** — insert pin headers into the
   perfboard, place MUX boards on top, solder from the bottom.

5. **Run power and ground bus wires** — red for 3V3, black for GND, on the
   bottom side.

6. **Run signal wires** — GPIO1, GPIO9, S0-S3, EN on the bottom side.

7. **Solder screw terminals** — insert from the top along both edges,
   solder on the bottom.

8. **Run channel wires** — connect MUX channel pins to screw terminal pins
   on the bottom side.

9. **Final check** — use multimeter to verify:
   - 3V3 bus to GND: no short (should read >10kΩ)
   - Junction node to GND: ~10kΩ (R2)
   - GPIO9 row to GND: ~1.01MΩ (R1 + R2 in series)
   - Each channel terminal to MUX channel pin: 0Ω (continuity)

---

## Testing Before Deploying

1. Plug in XIAO via USB-C
2. Flash ESPHome YAML
3. Connect one known probe pair to CH0 terminal
4. Verify log shows real voltage readings that differ from unconnected channels
5. Compare MC% reading against handheld meter
6. Once stable, connect remaining probe pairs
