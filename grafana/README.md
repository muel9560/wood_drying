# Grafana dashboards

Built for Grafana 11.6 + the InfluxDB 2.9 (Flux) datasource, against the schema
in [`../data/README.md`](../data/README.md).

| file | what it shows |
|---|---|
| `wood-stacks-overview.json` | Climate (temperature, humidity, EMC) per stack/level, plus board MC over time and a board list with sparkline + latest MC |
| `drying-control.json` | The two guardrails: drying rate (%/week, 1%/week line) and shell−core differential (shallow − deep, 4/5% bands) |

## Import

Dashboards → New → Import → upload the JSON. Grafana will prompt for the
**InfluxDB datasource** (`DS_INFLUXDB`). After import, pick the **`bucket`**
variable (top of the dashboard) if the default isn't your wood-data bucket.

Variables: `bucket`, `stack` (multi), `thickness` (multi) — all auto-populate
from the data.

## Two things to check

1. **EMC temperature units.** The EMC panel assumes the climate `temperature`
   field is **°F**. If your SHTCx nodes report °C, edit the EMC query and add
   `T = r.temperature * 9.0 / 5.0 + 32.0` before the equation (and set the
   Temperature panel unit to `celsius`).
2. **Reliable window.** Board MC is trustworthy ~7–25%; below ~7% it floors and
   very wet wood clamps. The drying-rate and differential panels are meaningful
   across that window.

## Not yet built (candidates)

- **MC − EMC driving force** (cross-measurement join of board MC to area EMC).
- **Days-to-target** projection (regression over a trailing window).
- Alerting — wire in Grafana or checkmk off the rate/differential series.
