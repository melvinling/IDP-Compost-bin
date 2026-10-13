#include "History.h"
void History::append(const SensorSnapshot &s) {
  const Reading *r[] = {&s.distance, &s.ec, &s.ph, &s.co2, &s.moisture, &s.temperature};
  HistorySample &row = rows_[head_];
  row.sequence = s.sequence; row.uptimeMs = s.uptimeMs;
  for (unsigned i=0; i<6; ++i) row.values[i] = r[i]->valid() ? r[i]->value : NAN;
  head_ = (head_+1)%Config::HISTORY_SIZE;
  if (count_ < Config::HISTORY_SIZE) ++count_;
}
