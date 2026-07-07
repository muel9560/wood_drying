# Grafana dashboards

Built for Grafana 11.6 + the InfluxDB 2.9 (Flux) datasource, against the schema
in [`../data/README.md`](../data/README.md).

| file | what it shows |
|---|---|
| `wood-stacks-overview.json` | Climate (temperature, humidity, EMC) per stack/level, plus board MC over time and a board list with sparkline + latest MC |
| `drying-control.json` | The two guardrails: drying rate (%/week, 1%/week line) and shell−core differential (shallow − deep, 4/5% bands) |

## Import

Dashboards → New → Import → upload the JSON. At the top of the dashboard set the
variables **left to right**: pick your InfluxDB **Data source**, then the
**Bucket**, then `stack` / `thickness` populate from the data.

The datasource is a real template variable (`${datasource}`) rather than an
import-time `__inputs` prompt — that's deliberate, because the `__inputs` style
often leaves *variable* queries pointed at an unresolved datasource on import
(the "imported dashboard, broken variables" problem). Everything here — panels
and variable queries — references `${datasource}`.

### If a variable query still returns nothing

1. **Datasource query language must be Flux.** Config → Data sources → your
   InfluxDB → the "Query language" must be **Flux**, not InfluxQL. Flux queries
   silently return nothing under InfluxQL. (If you're locked to InfluxQL, tell me
   and I'll rewrite the queries.)
2. **Test the bucket query in Influx Data Explorer as a table.** `buckets()` has
   no `_time`, so the Explorer's graph view shows nothing — use Script Editor →
   Submit → **Table / Raw Data**:
   ```
   buckets() |> filter(fn: (r) => r.name !~ /^_/) |> rename(columns: {name: "_value"}) |> keep(columns: ["_value"])
   ```
3. Set the **Bucket** value first — `stack`/`thickness` depend on it via
   `schema.tagValues(bucket: "${bucket}", …)`.

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
