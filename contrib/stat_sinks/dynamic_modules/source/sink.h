#pragma once

#include <string>
#include <vector>

#include "envoy/stats/sink.h"

#include "contrib/stat_sinks/dynamic_modules/source/sink_config.h"

namespace Envoy {
namespace Extensions {
namespace StatSinks {
namespace DynamicModules {

/**
 * Context passed as the snapshot_envoy_ptr during flush. Holds the snapshot pointer and
 * string caches. Every stat's name() and TextReadout::value() return std::string by value,
 * so we materialize them once per flush to hand modules stable pointers.
 */
struct StatSinkFlushContext {
  Stats::MetricSnapshot* snapshot;
  std::vector<std::string> counter_names;
  std::vector<std::string> gauge_names;
  std::vector<std::string> text_readout_names;
  std::vector<std::string> text_readout_values;
};

/**
 * Stats sink that delegates to a dynamic module. A single config is shared across all
 * instances; the sink itself holds no per-instance state beyond the config pointer.
 */
class DynamicModuleStatsSink : public Stats::Sink {
public:
  explicit DynamicModuleStatsSink(DynamicModuleStatsSinkConfigSharedPtr config);

  // Stats::Sink
  void flush(Stats::MetricSnapshot& snapshot) override;
  void onHistogramComplete(const Stats::Histogram& histogram, uint64_t value) override;

  /**
   * Helper to obtain the raw this pointer for use as the sink_envoy_ptr ABI argument.
   */
  void* thisAsVoidPtr() { return static_cast<void*>(this); }

private:
  DynamicModuleStatsSinkConfigSharedPtr config_;
};

} // namespace DynamicModules
} // namespace StatSinks
} // namespace Extensions
} // namespace Envoy
