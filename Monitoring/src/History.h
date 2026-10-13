#pragma once
#include "Types.h"
#include "Config.h"
struct HistorySample { uint32_t sequence; uint64_t uptimeMs; float values[6]; };
class History {
 public:
  void append(const SensorSnapshot &s);
  size_t count() const { return count_; }
  const HistorySample &at(size_t chronologicalIndex) const { return rows_[(head_+Config::HISTORY_SIZE-count_+chronologicalIndex)%Config::HISTORY_SIZE]; }
 private:
  HistorySample rows_[Config::HISTORY_SIZE] = {};
  size_t head_ = 0, count_ = 0;
};
