#include "envoy/stats/sink.h"

#include "source/extensions/dynamic_modules/abi/abi.h"

#include "contrib/stat_sinks/dynamic_modules/source/sink.h"

namespace Envoy {
namespace Extensions {
namespace StatSinks {
namespace DynamicModules {

namespace {

StatSinkFlushContext* toContext(envoy_dynamic_module_type_stat_sink_snapshot_envoy_ptr ptr) {
  return static_cast<StatSinkFlushContext*>(ptr);
}

} // namespace

extern "C" {

size_t envoy_dynamic_module_callback_stat_sink_snapshot_get_counter_count(
    envoy_dynamic_module_type_stat_sink_snapshot_envoy_ptr snapshot_envoy_ptr) {
  return toContext(snapshot_envoy_ptr)->snapshot->counters().size();
}

bool envoy_dynamic_module_callback_stat_sink_snapshot_get_counter(
    envoy_dynamic_module_type_stat_sink_snapshot_envoy_ptr snapshot_envoy_ptr, size_t index,
    envoy_dynamic_module_type_envoy_buffer* name_out, uint64_t* value_out, uint64_t* delta_out) {
  auto* ctx = toContext(snapshot_envoy_ptr);
  const auto& counters = ctx->snapshot->counters();
  if (index >= counters.size()) {
    return false;
  }
  const auto& snap = counters[index];
  const std::string& name = ctx->counter_names[index];
  *name_out = {.ptr = name.data(), .length = name.size()};
  *value_out = snap.counter_.get().value();
  *delta_out = snap.delta_;
  return true;
}

size_t envoy_dynamic_module_callback_stat_sink_snapshot_get_gauge_count(
    envoy_dynamic_module_type_stat_sink_snapshot_envoy_ptr snapshot_envoy_ptr) {
  return toContext(snapshot_envoy_ptr)->snapshot->gauges().size();
}

bool envoy_dynamic_module_callback_stat_sink_snapshot_get_gauge(
    envoy_dynamic_module_type_stat_sink_snapshot_envoy_ptr snapshot_envoy_ptr, size_t index,
    envoy_dynamic_module_type_envoy_buffer* name_out, uint64_t* value_out) {
  auto* ctx = toContext(snapshot_envoy_ptr);
  const auto& gauges = ctx->snapshot->gauges();
  if (index >= gauges.size()) {
    return false;
  }
  const Stats::Gauge& gauge = gauges[index].get();
  const std::string& name = ctx->gauge_names[index];
  *name_out = {.ptr = name.data(), .length = name.size()};
  *value_out = gauge.value();
  return true;
}

size_t envoy_dynamic_module_callback_stat_sink_snapshot_get_text_readout_count(
    envoy_dynamic_module_type_stat_sink_snapshot_envoy_ptr snapshot_envoy_ptr) {
  return toContext(snapshot_envoy_ptr)->snapshot->textReadouts().size();
}

bool envoy_dynamic_module_callback_stat_sink_snapshot_get_text_readout(
    envoy_dynamic_module_type_stat_sink_snapshot_envoy_ptr snapshot_envoy_ptr, size_t index,
    envoy_dynamic_module_type_envoy_buffer* name_out,
    envoy_dynamic_module_type_envoy_buffer* value_out) {
  auto* ctx = toContext(snapshot_envoy_ptr);
  const auto& readouts = ctx->snapshot->textReadouts();
  if (index >= readouts.size()) {
    return false;
  }
  // Use the pre-cached strings to avoid dangling pointers from std::string temporaries.
  const std::string& name = ctx->text_readout_names[index];
  const std::string& value = ctx->text_readout_values[index];
  *name_out = {.ptr = name.data(), .length = name.size()};
  *value_out = {.ptr = value.data(), .length = value.size()};
  return true;
}

} // extern "C"

} // namespace DynamicModules
} // namespace StatSinks
} // namespace Extensions
} // namespace Envoy
