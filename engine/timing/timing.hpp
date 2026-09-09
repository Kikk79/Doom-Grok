#pragma once

namespace Timing {
  constexpr double kFixedDt = 1.0 / 60.0;

  // Accumulator-based fixed timestep helper.
  class FixedStep {
  public:
    void reset();
    // Add wall time; returns number of fixed steps to simulate (capped).
    int consume(double frame_seconds);
    double alpha() const;  // leftover / kFixedDt for interpolation
    double accumulator() const;
  private:
    double acc_ = 0.0;
    static constexpr int kMaxSteps = 5;
  };
}
