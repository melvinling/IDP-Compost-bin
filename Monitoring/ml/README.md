# Synthetic demonstration model

The firmware and simulator both execute `src/Prediction.cpp` with the checked-in
`src/GeneratedForest.h`. No Python runtime, cloud service or model download is
needed for inference.

The dataset contains 1,200 artificial snapshots. A seed-42 GroupShuffleSplit
keeps each artificial scenario group within one split: 900 training rows and
300 held-out rows. The deployed forest is the evaluated model; the held-out rows
were not used for a subsequent retraining.

| Model | Synthetic holdout accuracy | Macro F1 |
| --- | ---: | ---: |
| Random Forest, 32 trees / depth 6 | 99.67% | 0.99662 |
| Decision tree, depth 6 | 100% | 1.00000 |
| Rules used to create the labels | 100% | 1.00000 |

These scores measure imitation of artificial labels. They do not establish
which algorithm is best for real Bokashi, or whether the thresholds improve
compost outcomes. The rule baseline's perfect score is expected by construction.
Artificial group separation is useful for the pipeline but does not provide
independent biological experiments. Temperature, EC, pH, CO₂ and distance do not
determine these labels; a forest may still make spurious splits on them.

The six measurements plus three context features use this exact order:

```text
moisture_pct, temperature_c, ec_us_cm, ph, co2_ppm, distance_cm,
process_stage, periodic_mix_due, post_watering_mix_due
```

Stage: fermentation = 0, finishing = 1. Class probability order: idle, water,
dry, mix. IDs, source, group ID and target labels never enter the features.
The model chooses the largest probability, breaking ties by that class order.
Confidence is an uncalibrated class probability. The controller separately
enforces stage restrictions, freshness, stops, timing, interlocks and cooldowns.
Invalid measurements return an unavailable prediction and OFF requests.

The export stores 1,578 nodes, double-precision thresholds and normalized float
leaf probabilities. Inputs use float32 as in sklearn. `ml/parity.csv` contains
4,519 cases: all data, 1,000 random probes and float32 threshold neighbours.
The C++ test compares all four probabilities with tolerance 0.000002.

To reproduce the model from `Monitoring`:

```powershell
python -m pip install -r ml\requirements.txt
python ml\train.py
.\tools\test.ps1
```

Training overwrites the generated header, report and parity fixture. The report
records the dataset SHA-256, library version, features, class mapping, split
groups, parameters, importances, confusion matrices and scores. It does not
save an executable pickle; the C++ header is the deployment artifact.

For a real model, collect calibrated measurements, actual batch/session IDs,
reviewed actions and measured outcomes. Split by independent batches and time,
compare against a calibrated rule controller, and evaluate failures separately.
The current synthetic threshold of below 40% for water and above 60% for dry
is a demonstration assumption, not a physical operating recommendation.
