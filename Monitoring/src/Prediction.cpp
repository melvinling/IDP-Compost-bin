#include "Prediction.h"
#include "GeneratedForest.h"
void modelProbabilities(const float features[9], float probabilities[4]) {
  for (unsigned c = 0; c < 4; ++c) probabilities[c] = 0;
  for (unsigned t = 0; t < Forest::TREE_COUNT; ++t) {
    unsigned i = Forest::roots[t];
    while (Forest::nodes[i].feature >= 0) {
      const auto &n = Forest::nodes[i];
      i = features[n.feature] <= n.threshold ? n.left : n.right;
    }
    for (unsigned c = 0; c < 4; ++c) probabilities[c] += Forest::nodes[i].probabilities[c] / Forest::TREE_COUNT;
  }
}
PredictionResult predictActuators(const SensorSnapshot &s, const ControlContext &context) {
  PredictionResult p; p.contextRevision = context.revision;
  if (!s.valid()) { p.reason = "Synthetic RF: missing or invalid input"; return p; }
  const float x[] = {s.moisture.value, s.temperature.value, s.ec.value, s.ph.value, s.co2.value, s.distance.value,
    float(context.stage == Stage::Finishing), float(context.periodicMixDue), float(context.postWaterMixDue)};
  float probability[4]; modelProbabilities(x, probability);
  unsigned best = 0; for (unsigned c = 1; c < 4; ++c) if (probability[c] > probability[best]) best = c;
  p.modelReady = true; p.reason = "Synthetic demonstration RF; hardware calibration required";
  p.action = static_cast<Decision>(best); p.confidence = probability[best];
  p.command = {p.action == Decision::Mix, p.action == Decision::Water, p.action == Decision::Dry};
  return p;
}
