#include "timing.hpp"
#include <algorithm>

namespace Timing {

void FixedStep::reset() { acc_ = 0.0; }

int FixedStep::consume(double frame_seconds) {
  if (frame_seconds < 0.0) frame_seconds = 0.0;
  // Clamp huge hitch so we don't spiral
  if (frame_seconds > 0.25) frame_seconds = 0.25;
  acc_ += frame_seconds;
  int steps = 0;
  while (acc_ >= kFixedDt && steps < kMaxSteps) {
    acc_ -= kFixedDt;
    ++steps;
  }
  if (steps == kMaxSteps && acc_ > kFixedDt * 2.0) {
    acc_ = 0.0;  // drop leftover after long stall
  }
  return steps;
}

double FixedStep::alpha() const {
  return std::clamp(acc_ / kFixedDt, 0.0, 1.0);
}

double FixedStep::accumulator() const { return acc_; }

}  // namespace Timing
