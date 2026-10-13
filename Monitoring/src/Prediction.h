#pragma once
#include "Types.h"
PredictionResult predictActuators(const SensorSnapshot &readings, const ControlContext &context = {});
void modelProbabilities(const float features[9], float probabilities[4]);
