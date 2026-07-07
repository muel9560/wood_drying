# Wood-Stack Data Schema — StatsD → Telegraf → InfluxDB

How the sensor data is named, tagged, and stored. Everything lands in a single
InfluxDB measurement (`woodstack`); the two sensor kinds are separated by tags,
not by measurement.

## Pipeline

```
ESPHome nodes ──UDP:8125──▶ Telegraf statsd input ──▶ InfluxDB 2.9 ──▶ Grafana 11.6
                            (40-statsd.conf)          measurement: woodstack
```

- `40-statsd.conf` — the Telegraf input + templates in this directory.
- `influxdb-statsd-woodstack-15min.csv` — a sample export for reference.

## Sensor kinds

| `kind` | nodes | what it measures |
|---|---|---|
| `wood` | woodstack01-mer | pin-pair resistance → moisture content, per channel |
| `climate` | woodstack02/03/10-mer | SHTCx air temperature + humidity in the stack |

## Metric name structure

The StatsD bucket name is parsed positionally by the Telegraf templates. The
literal `wood` / `climate` in the 4th segment selects the template.

```
wood:     woodstack.esp32.<node>.wood.<stack>.<level>.<thickness>.<board>.<spot>.<depth>.<field>
climate:  woodstack.esp32.<node>.climate.<stack>.<level>.<field>
```

## Tags

| tag | kind | values | notes |
|---|---|---|---|
| `kind` | both | `wood` \| `climate` | the hard separator between the two sensor types |
| `sensorplatform` | both | `esp32` | |
| `sensorid` | both | `woodstack01-mer` … | the physical ESP node |
| `stack` | both | `shelf1`, `stack1`, `stack2` | which physical stack (`shelf1` = the Husky shelving) |
| `level` | both | `s1`–`s4` (shelving) or `high`/`mid`/`low` (slab stacks) | vertical position; a coarse `high/mid/low` **tier** is derived in Grafana |
| `thickness` | wood | `5-4`, `6-4` … `13-4`; `cal` (bench pieces) | dash, not slash — `/` and `.` are illegal in StatsD names |
| `board` | wood | stable id within a (stack, thickness), e.g. `01`, `02`, or a name | |
| `spot` | wood | `mid` \| `end1` \| `end2` | small boards use `mid`; wide/long slabs get two probe sets at ⅓ from each end |
| `depth` | wood | `shallow` \| `deep` | ¼-depth vs ½-depth pin pair |

Telegraf also attaches its own global tags (`host`, `location`, `model`,
`metric_type`, `type`) — leave those as-is.

## Fields

| `_field` | kind | unit | notes |
|---|---|---|---|
| `mc` | wood | % | moisture content from the calibration curve |
| `resistance` | wood | Ω | raw pin-pair resistance (pre-clamp). **Always shipped** so MC can be re-derived in Grafana with an updated curve without reflashing |
| `temperature` | climate | °C/°F | keep units consistent across nodes |
| `humidity` | climate | % RH | |

Per-channel `temp` and Wi-Fi diagnostics are intentionally **not** sent to
StatsD (temp is redundant with the climate nodes; Wi-Fi is noise here).

## Telegraf templates (`40-statsd.conf`)

```toml
templates = [
  "woodstack.*.*.wood.*.*.*.*.*.*.* measurement.sensorplatform.sensorid.kind.stack.level.thickness.board.spot.depth.field",
  "woodstack.*.*.climate.*.*.*     measurement.sensorplatform.sensorid.kind.stack.level.field",
  "sensor.*.*.*.*.*.*.*.*          measurement.sensorplatform.loc.building.floor.room.sensorid.sensortype.field",
]
```

The wood (11-segment) and climate (7-segment) filters can't collide; the third
template is the unrelated MER sensor fleet and is left untouched.

## ESPHome naming

Set the per-sensor `name` under `statsd:` (the `prefix` supplies the first three
segments `woodstack.esp32.<node>`).

```yaml
# moisture node (woodstack01-mer), prefix: woodstack.esp32.woodstack01-mer
- { name: "wood.shelf1.s4.5-4.01.mid.shallow.mc",         id: m_a00 }
- { name: "wood.shelf1.s4.5-4.01.mid.shallow.resistance", id: m_a00_r }

# climate node (woodstack10-mer), prefix: woodstack.esp32.woodstack10-mer
- { name: "climate.shelf1.s4.temperature", id: shtcx_temp }
- { name: "climate.shelf1.s4.humidity",    id: shtcx_hum }
```

## Worked example

`woodstack.esp32.woodstack01-mer.wood.shelf1.s4.5-4.01.mid.shallow.mc` →

| tag/field | value |
|---|---|
| `_measurement` | woodstack |
| `sensorid` | woodstack01-mer |
| `kind` | wood |
| `stack` | shelf1 |
| `level` | s4 |
| `thickness` | 5-4 |
| `board` | 01 |
| `spot` | mid |
| `depth` | shallow |
| `_field` | mc |

Grafana legend: `{{thickness}} · b{{board}} · {{spot}} · {{depth}} · {{stack}}/{{level}}`
→ **"5-4 · b01 · mid · shallow · shelf1/s4"**.

## Sample cadence

| kind | interval | why |
|---|---|---|
| wood | 5 min | drying is slow; leaves time for all 16 channels; gentler on the probes |
| climate | 15 s | air conditions change faster and drive the drying rate |

## Notes

- **Old-schema data** (before 2026-07-07) landed moisture with `mc`/`resistance`
  as a `sensortype` tag and the number in a generic `value` field. New data uses
  `kind` + a proper `_field`. Query the new schema for dashboards; the old points
  age out with retention.
- The coarse **tier** (`high`/`mid`/`low`) for cross-stack comparison is derived
  in Grafana from `level` — it is not stored as a tag.
