// NOLINT(namespace-envoy)
//
// Example WASM stats filter plugin for envoy.stat_sinks.wasm_filter.
//
// Demonstrates:
//   - Reading node metadata at startup to compute global tags
//   - Filtering metrics by name prefix
//   - Filtering histograms via stats_filter_get_histograms
//   - Renaming metrics (prefix with "envoy.")
//   - Injecting synthetic metrics (metricsPublished count)
//
// Configuration (passed via PluginConfig.configuration as StringValue):
//   {"exclude_prefixes": ["server.compilation", "runtime."]}

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#ifndef NULL_PLUGIN
#include "proxy_wasm_intrinsics.h"

#include "source/extensions/common/wasm/ext/envoy_proxy_wasm_api.h"
#else
#include "source/extensions/common/wasm/ext/envoy_null_plugin.h"
#endif

START_WASM_PLUGIN(StatsFilterPlugin)

struct HistogramInfo {
  std::string name;
};

class StatsFilterRootContext : public EnvoyRootContext {
public:
  explicit StatsFilterRootContext(uint32_t id, std::string_view root_id)
      : EnvoyRootContext(id, root_id) {}

  bool onConfigure(size_t config_size) override;
  void onStatsUpdate(uint32_t result_size) override;

private:
  bool shouldExclude(std::string_view name) const;
  std::vector<HistogramInfo> fetchHistograms() const;
  void setGlobalTagsFromNodeMetadata();

  std::vector<std::string> exclude_prefixes_;
  bool rename_with_prefix_{false};
};

class StatsFilterContext : public EnvoyContext {
public:
  explicit StatsFilterContext(uint32_t id, RootContext* root) : EnvoyContext(id, root) {}
};

static RegisterContextFactory register_StatsFilterContext(CONTEXT_FACTORY(StatsFilterContext),
                                                          ROOT_FACTORY(StatsFilterRootContext));

void StatsFilterRootContext::setGlobalTagsFromNodeMetadata() {
  // Read node metadata to build global tags: datacenter, superpod, pod, device.
  // These are available via getProperty({"xds", "node", "metadata", "<key>"}).
  std::vector<std::pair<std::string, std::vector<std::string>>> tag_sources = {
      {"datacenter", {"xds", "node", "metadata", "datacenter"}},
      {"superpod", {"xds", "node", "metadata", "superpod"}},
      {"pod", {"xds", "node", "metadata", "pod"}},
      {"device", {"xds", "node", "id"}},
  };

  // Build wire format: [tag_count: u32] for each: [name_len][name][value_len][value]
  std::vector<std::pair<std::string, std::string>> tags;
  for (const auto& [tag_name, path] : tag_sources) {
    std::string value;
    if (getValue(path, &value) && !value.empty()) {
      tags.emplace_back(tag_name, value);
    }
  }

  if (tags.empty()) {
    return;
  }

  // Serialize the tags.
  std::string wire;
  uint32_t count = tags.size();
  wire.append(reinterpret_cast<const char*>(&count), sizeof(uint32_t));
  for (const auto& [name, value] : tags) {
    uint32_t name_len = name.size();
    wire.append(reinterpret_cast<const char*>(&name_len), sizeof(uint32_t));
    wire.append(name);
    uint32_t value_len = value.size();
    wire.append(reinterpret_cast<const char*>(&value_len), sizeof(uint32_t));
    wire.append(value);
  }

  char* result = nullptr;
  size_t result_size = 0;
  proxy_call_foreign_function("stats_filter_set_global_tags", wire.data(), wire.size(), &result,
                              &result_size);
  logInfo("StatsFilterPlugin: set " + std::to_string(tags.size()) +
          " global tags from node metadata");
}

bool StatsFilterRootContext::onConfigure(size_t config_size) {
  // Set global tags from node metadata (once at startup).
  setGlobalTagsFromNodeMetadata();

  if (config_size == 0) {
    return true;
  }
  auto config_data = getBufferBytes(WasmBufferType::PluginConfiguration, 0, config_size);
  auto config = config_data->toString();

  // Parse exclude_prefixes.
  size_t pos = config.find("\"exclude_prefixes\"");
  if (pos != std::string::npos) {
    pos = config.find('[', pos);
    if (pos != std::string::npos) {
      size_t end = config.find(']', pos);
      if (end != std::string::npos) {
        std::string array_content = config.substr(pos + 1, end - pos - 1);
        size_t search_pos = 0;
        while (true) {
          size_t quote_start = array_content.find('"', search_pos);
          if (quote_start == std::string::npos)
            break;
          size_t quote_end = array_content.find('"', quote_start + 1);
          if (quote_end == std::string::npos)
            break;
          exclude_prefixes_.push_back(
              array_content.substr(quote_start + 1, quote_end - quote_start - 1));
          search_pos = quote_end + 1;
        }
      }
    }
  }

  // Parse rename_with_prefix.
  if (config.find("\"rename_with_prefix\": true") != std::string::npos ||
      config.find("\"rename_with_prefix\":true") != std::string::npos) {
    rename_with_prefix_ = true;
  }

  logInfo("StatsFilterPlugin: configured with " + std::to_string(exclude_prefixes_.size()) +
          " exclude prefixes, rename=" + (rename_with_prefix_ ? "true" : "false"));
  return true;
}

bool StatsFilterRootContext::shouldExclude(std::string_view name) const {
  for (const auto& prefix : exclude_prefixes_) {
    if (name.size() >= prefix.size() && name.substr(0, prefix.size()) == prefix) {
      return true;
    }
  }
  return false;
}

std::vector<HistogramInfo> StatsFilterRootContext::fetchHistograms() const {
  char* result = nullptr;
  size_t result_size = 0;
  auto status =
      proxy_call_foreign_function("stats_filter_get_histograms", nullptr, 0, &result, &result_size);
  if (status != WasmResult::Ok || result == nullptr || result_size < sizeof(uint32_t)) {
    return {};
  }

  std::vector<HistogramInfo> histograms;
  size_t offset = 0;
  uint32_t count = 0;
  memcpy(&count, result + offset, sizeof(uint32_t));
  offset += sizeof(uint32_t);

  for (uint32_t i = 0; i < count && offset < result_size; ++i) {
    uint32_t name_len = 0;
    memcpy(&name_len, result + offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    histograms.push_back({std::string(result + offset, name_len)});
    offset += name_len;
  }

  free(result);
  return histograms;
}

void StatsFilterRootContext::onStatsUpdate(uint32_t result_size) {
  auto stats_buffer = getBufferBytes(WasmBufferType::CallData, 0, result_size);
  auto stats = parseStatResults(stats_buffer->view());

  // -- Filter and optionally rename counters --
  std::vector<uint32_t> kept_counter_indices;
  std::vector<std::tuple<uint32_t, uint32_t, std::string>> name_overrides; // (type, idx, name)

  for (uint32_t i = 0; i < stats.counters.size(); ++i) {
    if (!shouldExclude(stats.counters[i].name)) {
      kept_counter_indices.push_back(i);
      if (rename_with_prefix_) {
        name_overrides.emplace_back(1, i, "envoy." + std::string(stats.counters[i].name));
      }
    }
  }

  // -- Filter and optionally rename gauges --
  std::vector<uint32_t> kept_gauge_indices;
  for (uint32_t i = 0; i < stats.gauges.size(); ++i) {
    if (!shouldExclude(stats.gauges[i].name)) {
      kept_gauge_indices.push_back(i);
      if (rename_with_prefix_) {
        name_overrides.emplace_back(2, i, "envoy." + std::string(stats.gauges[i].name));
      }
    }
  }

  // -- Fetch and filter histograms --
  auto histograms = fetchHistograms();
  std::vector<uint32_t> kept_histogram_indices;
  for (uint32_t i = 0; i < histograms.size(); ++i) {
    if (!shouldExclude(histograms[i].name)) {
      kept_histogram_indices.push_back(i);
      if (rename_with_prefix_) {
        name_overrides.emplace_back(3, i, "envoy." + histograms[i].name);
      }
    }
  }

  // -- Set name overrides --
  if (!name_overrides.empty()) {
    std::string wire;
    uint32_t count = name_overrides.size();
    wire.append(reinterpret_cast<const char*>(&count), sizeof(uint32_t));
    for (const auto& [type, idx, name] : name_overrides) {
      wire.append(reinterpret_cast<const char*>(&type), sizeof(uint32_t));
      wire.append(reinterpret_cast<const char*>(&idx), sizeof(uint32_t));
      uint32_t name_len = name.size();
      wire.append(reinterpret_cast<const char*>(&name_len), sizeof(uint32_t));
      wire.append(name);
    }
    char* nr = nullptr;
    size_t nrs = 0;
    proxy_call_foreign_function("stats_filter_set_name_overrides", wire.data(), wire.size(), &nr,
                                &nrs);
  }

  // -- Inject synthetic metrics --
  {
    std::string wire;
    auto appendU32 = [&](uint32_t v) {
      wire.append(reinterpret_cast<const char*>(&v), sizeof(uint32_t));
    };
    auto appendU64 = [&](uint64_t v) {
      wire.append(reinterpret_cast<const char*>(&v), sizeof(uint64_t));
    };
    auto appendStr = [&](const std::string& s) {
      appendU32(s.size());
      wire.append(s);
    };

    // 1 synthetic counter: wasm_filter.metrics_emitted = total kept metrics count
    appendU32(1);
    appendStr("wasm_filter.metrics_emitted");
    uint64_t total =
        kept_counter_indices.size() + kept_gauge_indices.size() + kept_histogram_indices.size();
    appendU64(total);
    appendU32(0); // no per-metric tags

    // 0 synthetic gauges
    appendU32(0);

    char* ir = nullptr;
    size_t irs = 0;
    proxy_call_foreign_function("stats_filter_inject_metrics", wire.data(), wire.size(), &ir, &irs);
  }

  // -- Emit kept indices --
  {
    std::vector<uint32_t> wire;
    wire.reserve(3 + kept_counter_indices.size() + kept_gauge_indices.size() +
                 kept_histogram_indices.size());

    wire.push_back(static_cast<uint32_t>(kept_counter_indices.size()));
    for (auto idx : kept_counter_indices)
      wire.push_back(idx);
    wire.push_back(static_cast<uint32_t>(kept_gauge_indices.size()));
    for (auto idx : kept_gauge_indices)
      wire.push_back(idx);
    wire.push_back(static_cast<uint32_t>(kept_histogram_indices.size()));
    for (auto idx : kept_histogram_indices)
      wire.push_back(idx);

    char* emit_result = nullptr;
    size_t emit_result_size = 0;
    proxy_call_foreign_function("stats_filter_emit", reinterpret_cast<const char*>(wire.data()),
                                wire.size() * sizeof(uint32_t), &emit_result, &emit_result_size);
  }
}

END_WASM_PLUGIN
