#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "envoy/stats/sink.h"

#include "source/extensions/common/wasm/wasm.h"
#include "source/extensions/stat_sinks/wasm_filter/enriched_metric.h"

#include "absl/container/flat_hash_set.h"

namespace Envoy {
namespace Extensions {
namespace StatSinks {
namespace WasmFilter {

// Per-metric name override: (type, index) -> new name.
// type: 1=counter, 2=gauge, 3=histogram.
struct NameOverride {
  uint32_t type;
  uint32_t index;
  std::string new_name;
};

// Synthetic metric definition received from the WASM plugin.
struct SyntheticMetricDef {
  std::string name;
  uint64_t value;
  Stats::TagVector tags;
};

// Thread-local context for the WASM filter, holding filter decisions, name
// overrides, synthetic metrics, and snapshot access for foreign functions.
struct StatsFilterContext {
  absl::flat_hash_set<uint32_t> kept_counter_indices;
  absl::flat_hash_set<uint32_t> kept_gauge_indices;
  absl::flat_hash_set<uint32_t> kept_histogram_indices;

  // Per-flush name overrides set by stats_filter_set_name_overrides.
  std::vector<NameOverride> name_overrides;

  // Per-flush synthetic metrics set by stats_filter_inject_metrics.
  std::vector<SyntheticMetricDef> synthetic_counters;
  std::vector<SyntheticMetricDef> synthetic_gauges;

  // Non-owning pointer to the snapshot currently being flushed.
  Stats::MetricSnapshot* snapshot{};

  // Maps buffer-order index → snapshot-order index. Built before calling into
  // WASM because onStatsUpdate only serializes used() metrics, but
  // EnrichedMetricSnapshot indexes into the full snapshot arrays.
  std::vector<uint32_t> counter_buffer_to_snapshot;
  std::vector<uint32_t> gauge_buffer_to_snapshot;

  void clear() {
    kept_counter_indices.clear();
    kept_gauge_indices.clear();
    kept_histogram_indices.clear();
    name_overrides.clear();
    synthetic_counters.clear();
    synthetic_gauges.clear();
    counter_buffer_to_snapshot.clear();
    gauge_buffer_to_snapshot.clear();
    snapshot = nullptr;
  }
};

// Returns/sets the thread-local context pointer.
StatsFilterContext* getActiveContext();
void setActiveContext(StatsFilterContext* ctx);

// Global tags set once by the WASM plugin at startup via
// stats_filter_set_global_tags. Stored in the sink and shared by reference
// with all enriched metric wrappers.
// Thread-local accessor for the global tags (set by stats_filter_set_global_tags).
Stats::TagVector* getGlobalTags();
void setGlobalTags(Stats::TagVector* tags);

// Wraps an existing MetricSnapshot, applying:
//   - Filtering by kept indices
//   - Global tag injection on all metrics
//   - Per-metric name overrides
//   - Synthetic counter/gauge injection
class EnrichedMetricSnapshot : public Stats::MetricSnapshot {
public:
  EnrichedMetricSnapshot(Stats::MetricSnapshot& original, const StatsFilterContext& ctx,
                         const Stats::TagVector& global_tags);

  const std::vector<CounterSnapshot>& counters() override { return enriched_counters_; }
  const std::vector<std::reference_wrapper<const Stats::Gauge>>& gauges() override {
    return enriched_gauges_;
  }
  const std::vector<std::reference_wrapper<const Stats::ParentHistogram>>& histograms() override {
    return enriched_histograms_;
  }
  const std::vector<std::reference_wrapper<const Stats::TextReadout>>& textReadouts() override {
    return original_.textReadouts();
  }
  const std::vector<Stats::PrimitiveCounterSnapshot>& hostCounters() override {
    return original_.hostCounters();
  }
  const std::vector<Stats::PrimitiveGaugeSnapshot>& hostGauges() override {
    return original_.hostGauges();
  }
  SystemTime snapshotTime() const override { return original_.snapshotTime(); }

private:
  Stats::MetricSnapshot& original_;

  // Wrapper objects must outlive the snapshot. Stored here.
  std::vector<EnrichedCounter> counter_wrappers_;
  std::vector<EnrichedGauge> gauge_wrappers_;
  std::vector<EnrichedHistogram> histogram_wrappers_;
  std::vector<SyntheticCounter> synthetic_counter_objs_;
  std::vector<SyntheticGauge> synthetic_gauge_objs_;

  // Per-metric name overrides indexed by (type, original_index).
  std::vector<std::string> counter_name_overrides_;
  std::vector<std::string> gauge_name_overrides_;
  std::vector<std::string> histogram_name_overrides_;

  // Output vectors returned by accessors.
  std::vector<CounterSnapshot> enriched_counters_;
  std::vector<std::reference_wrapper<const Stats::Gauge>> enriched_gauges_;
  std::vector<std::reference_wrapper<const Stats::ParentHistogram>> enriched_histograms_;
};

// A stats sink that runs a WASM plugin as a filter/transformer/enricher before
// delegating to an inner sink.
class WasmFilterStatsSink : public Stats::Sink {
public:
  WasmFilterStatsSink(Common::Wasm::PluginConfigPtr plugin_config, Stats::SinkPtr inner_sink);

  void flush(Stats::MetricSnapshot& snapshot) override;

  void onHistogramComplete(const Stats::Histogram& histogram, uint64_t value) override {
    inner_sink_->onHistogramComplete(histogram, value);
  }

  Stats::TagVector& globalTags() { return global_tags_; }

private:
  Common::Wasm::PluginConfigPtr plugin_config_;
  Stats::SinkPtr inner_sink_;
  StatsFilterContext context_;
  Stats::TagVector global_tags_;
};

} // namespace WasmFilter
} // namespace StatSinks
} // namespace Extensions
} // namespace Envoy
