# Dummy Bokashi training dataset

`bokashi_dummy_training.csv` contains **1,200 generated examples**, with 300
examples for each action: `idle`, `water`, `dry`, and `mix`. All records identify
their source as `synthetic_demo`. The implementation now includes a demonstration
forest trained on a 900-row group split, with 300 rows held out; see
`../ml/report.json` and `../ml/README.md`. The original CSV remains unchanged.

One row represents an independent hypothetical sensor snapshot plus process
context. These rows are not real observations or a simulated fermentation time
series. They do not measure the biological benefit of any action.

## Columns

| Column | Meaning | Role |
| --- | --- | --- |
| `sample_id` | Unique example identifier | Metadata; exclude from model inputs |
| `data_source` | Always `synthetic_demo` | Metadata; exclude from model inputs |
| `scenario_group_id` | One of 40 artificial groups, 30 rows per group | Grouping metadata; exclude from model inputs |
| `process_stage` | `fermentation` or `finishing` | Categorical input |
| `moisture_pct` | Hypothetical probe-reported moisture percentage, 0-100 | Numeric input |
| `temperature_c` | Hypothetical temperature in degrees Celsius | Numeric input |
| `ec_us_cm` | Hypothetical electrical conductivity in microsiemens/cm | Numeric input |
| `ph` | Hypothetical pH, 3.0-9.0 | Numeric input |
| `co2_ppm` | Hypothetical CO2 concentration in ppm | Numeric input |
| `distance_cm` | Hypothetical ultrasonic distance to the material surface in cm | Numeric input |
| `periodic_mix_due` | 1 if a periodic mix is due; otherwise 0 | Context input |
| `post_watering_mix_due` | 1 if mixing after a completed watering pulse is pending; otherwise 0 | Context input |
| `target_action` | `idle`, `water`, `dry`, or `mix` | Classification target; never a model input |

For later numerical encoding, use fermentation = 0 and finishing = 1. Use
idle = 0, water = 1, dry = 2, mix = 3 for labels if an explicit numeric mapping
is needed. The CSV retains readable category names.

The dataset includes 150 fermentation records (all labelled `idle`) and 1,050
finishing records. Of the 300 mixing examples, 150 have post-watering mixing
pending and 150 have periodic mixing due with moisture in the idle band.

## Demonstration labelling policy

Apply these rules in this exact order:

1. During fermentation: `idle`, regardless of moisture or mixing flags.
2. During finishing with post-watering mixing pending: `mix`.
3. Otherwise, moisture below 40.0%: `water`.
4. Otherwise, moisture above 60.0%: `dry`.
5. Otherwise, when periodic mixing is due: `mix`.
6. Otherwise: `idle`.

Exactly 40.0% and 60.0% are inside the idle moisture band. Examples cover these
boundaries, 39.9%, 60.1%, and wet/dry cases with mixing flags to make precedence
explicit. Temperature, EC, pH, CO2, and distance vary but do not determine the
labels in this dummy policy. There is no experimentally supported relationship
between those measurements and these labels.

| Target action | Relay 1: fan | Relay 2: water pump | Relay 3: motor |
| --- | --- | --- | --- |
| `idle` | OFF | OFF | OFF |
| `water` | OFF | Requested | OFF |
| `dry` | Requested | OFF | OFF |
| `mix` | OFF | OFF | Requested |

These labels are requested actions, not evidence of physical actuator feedback
or permission to bypass stop commands, freshness checks, cooldowns, or runtime
limits. Those checks belong in the controller.

## Generation and intended use

Generated with a deterministic pseudorandom generator using seed 42, with
uniform sampling inside action-specific moisture bands and shuffled row order.
The other synthetic ranges are 18-40 C during fermentation, 15-65 C during
finishing, EC 0-10,000 uS/cm, CO2 400-8,000 ppm, and distance 5-100 cm. These are
illustrative sampling envelopes, not measured correlations or operating targets.
All numeric readings are finite; no missing or stale samples are included in
this training table. Sensor failure scenarios should be tested separately.

Use the file to prepare imports, feature encoding, training pipelines, and
software demonstrations. The 40%/60% thresholds are assumptions for this dummy
dataset; probe-reported volumetric moisture is not compost moisture by weight,
and these thresholds require calibration before physical use. A model trained
on these labels learns this demonstration policy, and does not establish
optimal Bokashi control.

If practising a group-based train/test split, keep each `scenario_group_id`
entirely within one split. These artificial groups are not real independent
experiments. Replace them with actual batch/session identifiers when collecting
real data, and use reviewed action labels and independent batches for evaluation.
