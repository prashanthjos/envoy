#include "source/extensions/stat_sinks/wasm_filter/wasm_filter_stat_sink_impl.h"

#include "test/mocks/stats/mocks.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

using testing::NiceMock;

namespace Envoy {
namespace Extensions {
namespace StatSinks {
namespace WasmFilter {
namespace {

class EnrichedMetricSnapshotTest : public testing::Test {
protected:
  void SetUp() override {
    counter_a_.name_ = "upstream_rq_2xx";
    counter_a_.latch_ = 10;
    counter_a_.used_ = true;

    counter_b_.name_ = "upstream_rq_5xx";
    counter_b_.latch_ = 5;
    counter_b_.used_ = true;

    counter_c_.name_ = "upstream_rq_total";
    counter_c_.latch_ = 15;
    counter_c_.used_ = true;

    gauge_a_.name_ = "membership_total";
    gauge_a_.value_ = 100;
    gauge_a_.used_ = true;

    gauge_b_.name_ = "connections_active";
    gauge_b_.value_ = 42;
    gauge_b_.used_ = true;

    histogram_a_.name_ = "upstream_rq_time";
    histogram_a_.used_ = true;

    histogram_b_.name_ = "downstream_rq_time";
    histogram_b_.used_ = true;

    snapshot_.counters_ = {{10, counter_a_}, {5, counter_b_}, {15, counter_c_}};
    snapshot_.gauges_ = {gauge_a_, gauge_b_};
    snapshot_.histograms_ = {histogram_a_, histogram_b_};

    ON_CALL(snapshot_, counters()).WillByDefault(testing::ReturnRef(snapshot_.counters_));
    ON_CALL(snapshot_, gauges()).WillByDefault(testing::ReturnRef(snapshot_.gauges_));
    ON_CALL(snapshot_, histograms()).WillByDefault(testing::ReturnRef(snapshot_.histograms_));
    ON_CALL(snapshot_, textReadouts()).WillByDefault(testing::ReturnRef(snapshot_.text_readouts_));
    ON_CALL(snapshot_, hostCounters()).WillByDefault(testing::ReturnRef(snapshot_.host_counters_));
    ON_CALL(snapshot_, hostGauges()).WillByDefault(testing::ReturnRef(snapshot_.host_gauges_));
  }

  StatsFilterContext makeCtxKeepAll() {
    StatsFilterContext ctx;
    ctx.kept_counter_indices = {0, 1, 2};
    ctx.kept_gauge_indices = {0, 1};
    ctx.kept_histogram_indices = {0, 1};
    ctx.snapshot = &snapshot_;
    return ctx;
  }

  NiceMock<Stats::MockCounter> counter_a_;
  NiceMock<Stats::MockCounter> counter_b_;
  NiceMock<Stats::MockCounter> counter_c_;
  NiceMock<Stats::MockGauge> gauge_a_;
  NiceMock<Stats::MockGauge> gauge_b_;
  NiceMock<Stats::MockParentHistogram> histogram_a_;
  NiceMock<Stats::MockParentHistogram> histogram_b_;
  NiceMock<Stats::MockMetricSnapshot> snapshot_;
  Stats::TagVector empty_tags_;
};

// -- Filtering tests --

TEST_F(EnrichedMetricSnapshotTest, KeepAll) {
  auto ctx = makeCtxKeepAll();
  EnrichedMetricSnapshot enriched(snapshot_, ctx, empty_tags_);

  EXPECT_EQ(enriched.counters().size(), 3);
  EXPECT_EQ(enriched.gauges().size(), 2);
  EXPECT_EQ(enriched.histograms().size(), 2);
}

TEST_F(EnrichedMetricSnapshotTest, DropAll) {
  StatsFilterContext ctx;
  ctx.kept_counter_indices = {};
  ctx.kept_gauge_indices = {};
  ctx.kept_histogram_indices = {999};
  ctx.snapshot = &snapshot_;
  EnrichedMetricSnapshot enriched(snapshot_, ctx, empty_tags_);

  EXPECT_EQ(enriched.counters().size(), 0);
  EXPECT_EQ(enriched.gauges().size(), 0);
  EXPECT_EQ(enriched.histograms().size(), 0);
}

TEST_F(EnrichedMetricSnapshotTest, KeepSubsetOfCounters) {
  StatsFilterContext ctx;
  ctx.kept_counter_indices = {0, 2};
  ctx.kept_gauge_indices = {0, 1};
  ctx.snapshot = &snapshot_;
  EnrichedMetricSnapshot enriched(snapshot_, ctx, empty_tags_);

  ASSERT_EQ(enriched.counters().size(), 2);
  EXPECT_EQ(enriched.counters()[0].counter_.get().name(), "upstream_rq_2xx");
  EXPECT_EQ(enriched.counters()[0].delta_, 10);
  EXPECT_EQ(enriched.counters()[1].counter_.get().name(), "upstream_rq_total");
}

TEST_F(EnrichedMetricSnapshotTest, EmptyHistogramIndicesPassAllThrough) {
  StatsFilterContext ctx;
  ctx.kept_counter_indices = {0};
  ctx.kept_gauge_indices = {0};
  ctx.snapshot = &snapshot_;
  EnrichedMetricSnapshot enriched(snapshot_, ctx, empty_tags_);

  EXPECT_EQ(enriched.histograms().size(), 2);
}

// -- Global tag injection tests --

TEST_F(EnrichedMetricSnapshotTest, GlobalTagsAppliedToAllMetrics) {
  Stats::TagVector global_tags = {{"datacenter", "us-east-1"}, {"pod", "pod42"}};
  auto ctx = makeCtxKeepAll();
  EnrichedMetricSnapshot enriched(snapshot_, ctx, global_tags);

  auto counter_tags = enriched.counters()[0].counter_.get().tags();
  EXPECT_GE(counter_tags.size(), 2);
  EXPECT_EQ(counter_tags[counter_tags.size() - 2].name_, "datacenter");
  EXPECT_EQ(counter_tags[counter_tags.size() - 2].value_, "us-east-1");
  EXPECT_EQ(counter_tags[counter_tags.size() - 1].name_, "pod");
  EXPECT_EQ(counter_tags[counter_tags.size() - 1].value_, "pod42");

  auto gauge_tags = enriched.gauges()[0].get().tags();
  EXPECT_GE(gauge_tags.size(), 2);
  EXPECT_EQ(gauge_tags[gauge_tags.size() - 1].name_, "pod");

  auto hist_tags = enriched.histograms()[0].get().tags();
  EXPECT_GE(hist_tags.size(), 2);
  EXPECT_EQ(hist_tags[hist_tags.size() - 1].name_, "pod");
}

// -- Name override tests --

TEST_F(EnrichedMetricSnapshotTest, NameOverrideOnCounter) {
  auto ctx = makeCtxKeepAll();
  ctx.name_overrides.push_back({1, 0, "envoy.upstream_rq_2xx"});
  EnrichedMetricSnapshot enriched(snapshot_, ctx, empty_tags_);

  EXPECT_EQ(enriched.counters()[0].counter_.get().name(), "envoy.upstream_rq_2xx");
  EXPECT_EQ(enriched.counters()[1].counter_.get().name(), "upstream_rq_5xx");
}

TEST_F(EnrichedMetricSnapshotTest, NameOverrideOnGauge) {
  auto ctx = makeCtxKeepAll();
  ctx.name_overrides.push_back({2, 1, "envoy.connections_active"});
  EnrichedMetricSnapshot enriched(snapshot_, ctx, empty_tags_);

  EXPECT_EQ(enriched.gauges()[1].get().name(), "envoy.connections_active");
}

TEST_F(EnrichedMetricSnapshotTest, NameOverrideOnHistogram) {
  auto ctx = makeCtxKeepAll();
  ctx.name_overrides.push_back({3, 0, "envoy.upstream_rq_time"});
  EnrichedMetricSnapshot enriched(snapshot_, ctx, empty_tags_);

  EXPECT_EQ(enriched.histograms()[0].get().name(), "envoy.upstream_rq_time");
}

// -- Synthetic metric injection tests --

TEST_F(EnrichedMetricSnapshotTest, SyntheticCounterInjected) {
  auto ctx = makeCtxKeepAll();
  ctx.synthetic_counters.push_back({"wasm_filter.metrics_emitted", 42, {}});
  EnrichedMetricSnapshot enriched(snapshot_, ctx, empty_tags_);

  ASSERT_EQ(enriched.counters().size(), 4);
  EXPECT_EQ(enriched.counters()[3].counter_.get().name(), "wasm_filter.metrics_emitted");
  EXPECT_EQ(enriched.counters()[3].counter_.get().value(), 42);
  EXPECT_EQ(enriched.counters()[3].delta_, 42);
}

TEST_F(EnrichedMetricSnapshotTest, SyntheticGaugeInjected) {
  auto ctx = makeCtxKeepAll();
  ctx.synthetic_gauges.push_back({"app.version", 1, {{"version", "v1.2.3"}}});
  EnrichedMetricSnapshot enriched(snapshot_, ctx, empty_tags_);

  ASSERT_EQ(enriched.gauges().size(), 3);
  EXPECT_EQ(enriched.gauges()[2].get().name(), "app.version");
  EXPECT_EQ(enriched.gauges()[2].get().value(), 1);
  auto tags = enriched.gauges()[2].get().tags();
  ASSERT_EQ(tags.size(), 1);
  EXPECT_EQ(tags[0].name_, "version");
  EXPECT_EQ(tags[0].value_, "v1.2.3");
}

TEST_F(EnrichedMetricSnapshotTest, SyntheticMetricsGetGlobalTags) {
  Stats::TagVector global_tags = {{"dc", "us-east-1"}};
  auto ctx = makeCtxKeepAll();
  ctx.synthetic_counters.push_back({"custom.count", 10, {}});
  EnrichedMetricSnapshot enriched(snapshot_, ctx, global_tags);

  auto last = enriched.counters().back();
  auto tags = last.counter_.get().tags();
  ASSERT_GE(tags.size(), 1);
  EXPECT_EQ(tags.back().name_, "dc");
}

// -- Combined test --

TEST_F(EnrichedMetricSnapshotTest, FilterRenameTagInjectCombined) {
  Stats::TagVector global_tags = {{"env", "prod"}};
  StatsFilterContext ctx;
  ctx.kept_counter_indices = {0};
  ctx.kept_gauge_indices = {1};
  ctx.snapshot = &snapshot_;
  ctx.name_overrides.push_back({1, 0, "envoy.upstream_rq_2xx"});
  ctx.synthetic_gauges.push_back({"wasm_filter.queue_depth", 7, {}});

  EnrichedMetricSnapshot enriched(snapshot_, ctx, global_tags);

  ASSERT_EQ(enriched.counters().size(), 1);
  EXPECT_EQ(enriched.counters()[0].counter_.get().name(), "envoy.upstream_rq_2xx");
  auto ctags = enriched.counters()[0].counter_.get().tags();
  EXPECT_EQ(ctags.back().name_, "env");

  ASSERT_EQ(enriched.gauges().size(), 2);
  EXPECT_EQ(enriched.gauges()[0].get().name(), "connections_active");
  EXPECT_EQ(enriched.gauges()[1].get().name(), "wasm_filter.queue_depth");

  EXPECT_EQ(enriched.histograms().size(), 2);
}

// -- StatsFilterContext tests --

TEST(StatsFilterContextAccessorTest, ThreadLocalAccessors) {
  EXPECT_EQ(getActiveContext(), nullptr);

  StatsFilterContext ctx;
  setActiveContext(&ctx);
  EXPECT_EQ(getActiveContext(), &ctx);

  setActiveContext(nullptr);
  EXPECT_EQ(getActiveContext(), nullptr);
}

TEST(StatsFilterContextAccessorTest, ClearResetsEverything) {
  StatsFilterContext ctx;
  ctx.kept_counter_indices.insert(1);
  ctx.kept_gauge_indices.insert(2);
  ctx.kept_histogram_indices.insert(3);
  ctx.name_overrides.push_back({1, 0, "foo"});
  ctx.synthetic_counters.push_back({"bar", 1, {}});
  ctx.synthetic_gauges.push_back({"baz", 2, {}});
  NiceMock<Stats::MockMetricSnapshot> snap;
  ctx.snapshot = &snap;

  ctx.clear();

  EXPECT_TRUE(ctx.kept_counter_indices.empty());
  EXPECT_TRUE(ctx.kept_gauge_indices.empty());
  EXPECT_TRUE(ctx.kept_histogram_indices.empty());
  EXPECT_TRUE(ctx.name_overrides.empty());
  EXPECT_TRUE(ctx.synthetic_counters.empty());
  EXPECT_TRUE(ctx.synthetic_gauges.empty());
  EXPECT_EQ(ctx.snapshot, nullptr);
}

// -- Foreign function wire format tests --

TEST(StatsFilterEmitTest, ParseCountersGaugesAndHistograms) {
  StatsFilterContext ctx;
  setActiveContext(&ctx);

  std::vector<uint32_t> wire = {2, 0, 2, 1, 1, 1, 0};
  std::string arguments(reinterpret_cast<const char*>(wire.data()),
                        wire.size() * sizeof(uint32_t));

  auto ff = proxy_wasm::getForeignFunction("stats_filter_emit");
  ASSERT_TRUE(ff != nullptr);

  proxy_wasm::WasmBase wasm_base(nullptr, "", "", "",
                                 std::unordered_map<std::string, std::string>{}, {});
  auto result = ff(wasm_base, arguments, [](size_t) -> void* { return nullptr; });
  EXPECT_EQ(result, proxy_wasm::WasmResult::Ok);

  EXPECT_EQ(ctx.kept_counter_indices.size(), 2);
  EXPECT_TRUE(ctx.kept_counter_indices.contains(0));
  EXPECT_TRUE(ctx.kept_counter_indices.contains(2));
  EXPECT_EQ(ctx.kept_gauge_indices.size(), 1);
  EXPECT_TRUE(ctx.kept_gauge_indices.contains(1));
  EXPECT_EQ(ctx.kept_histogram_indices.size(), 1);
  EXPECT_TRUE(ctx.kept_histogram_indices.contains(0));

  setActiveContext(nullptr);
}

TEST(StatsFilterEmitTest, HistogramBlockIsOptional) {
  StatsFilterContext ctx;
  setActiveContext(&ctx);

  std::vector<uint32_t> wire = {1, 0, 0};
  std::string arguments(reinterpret_cast<const char*>(wire.data()),
                        wire.size() * sizeof(uint32_t));

  auto ff = proxy_wasm::getForeignFunction("stats_filter_emit");
  ASSERT_TRUE(ff != nullptr);

  proxy_wasm::WasmBase wasm_base(nullptr, "", "", "",
                                 std::unordered_map<std::string, std::string>{}, {});
  auto result = ff(wasm_base, arguments, [](size_t) -> void* { return nullptr; });
  EXPECT_EQ(result, proxy_wasm::WasmResult::Ok);

  EXPECT_EQ(ctx.kept_counter_indices.size(), 1);
  EXPECT_TRUE(ctx.kept_histogram_indices.empty());

  setActiveContext(nullptr);
}

TEST(StatsFilterEmitTest, NoActiveContext) {
  setActiveContext(nullptr);

  std::vector<uint32_t> wire = {0, 0};
  std::string arguments(reinterpret_cast<const char*>(wire.data()),
                        wire.size() * sizeof(uint32_t));

  auto ff = proxy_wasm::getForeignFunction("stats_filter_emit");
  ASSERT_TRUE(ff != nullptr);

  proxy_wasm::WasmBase wasm_base(nullptr, "", "", "",
                                 std::unordered_map<std::string, std::string>{}, {});
  auto result = ff(wasm_base, arguments, [](size_t) -> void* { return nullptr; });
  EXPECT_EQ(result, proxy_wasm::WasmResult::InternalFailure);
}

TEST(StatsFilterEmitTest, EmptyArguments) {
  StatsFilterContext ctx;
  setActiveContext(&ctx);

  auto ff = proxy_wasm::getForeignFunction("stats_filter_emit");
  ASSERT_TRUE(ff != nullptr);

  proxy_wasm::WasmBase wasm_base(nullptr, "", "", "",
                                 std::unordered_map<std::string, std::string>{}, {});
  auto result = ff(wasm_base, "", [](size_t) -> void* { return nullptr; });
  EXPECT_EQ(result, proxy_wasm::WasmResult::BadArgument);

  setActiveContext(nullptr);
}

// -- Global tags foreign function test --

TEST(StatsFilterGlobalTagsTest, SetGlobalTags) {
  Stats::TagVector tags;
  setGlobalTags(&tags);

  // Wire: [2 tags] ["dc", 2 bytes] ["us", 2 bytes] ["pod", 3 bytes] ["p42", 3 bytes]
  std::string wire;
  uint32_t tag_count = 2;
  wire.append(reinterpret_cast<const char*>(&tag_count), sizeof(uint32_t));

  auto appendStr = [&](const std::string& s) {
    uint32_t len = s.size();
    wire.append(reinterpret_cast<const char*>(&len), sizeof(uint32_t));
    wire.append(s);
  };
  appendStr("dc");
  appendStr("us");
  appendStr("pod");
  appendStr("p42");

  auto ff = proxy_wasm::getForeignFunction("stats_filter_set_global_tags");
  ASSERT_TRUE(ff != nullptr);

  proxy_wasm::WasmBase wasm_base(nullptr, "", "", "",
                                 std::unordered_map<std::string, std::string>{}, {});
  auto result = ff(wasm_base, wire, [](size_t) -> void* { return nullptr; });
  EXPECT_EQ(result, proxy_wasm::WasmResult::Ok);

  ASSERT_EQ(tags.size(), 2);
  EXPECT_EQ(tags[0].name_, "dc");
  EXPECT_EQ(tags[0].value_, "us");
  EXPECT_EQ(tags[1].name_, "pod");
  EXPECT_EQ(tags[1].value_, "p42");

  setGlobalTags(nullptr);
}

// -- Name overrides foreign function test --

TEST(StatsFilterNameOverridesTest, SetNameOverrides) {
  StatsFilterContext ctx;
  setActiveContext(&ctx);

  // Wire: [1 override] [type=1] [index=0] [name="envoy.rq"]
  std::string wire;
  uint32_t count = 1;
  wire.append(reinterpret_cast<const char*>(&count), sizeof(uint32_t));
  uint32_t type = 1, index = 0;
  wire.append(reinterpret_cast<const char*>(&type), sizeof(uint32_t));
  wire.append(reinterpret_cast<const char*>(&index), sizeof(uint32_t));
  std::string name = "envoy.rq";
  uint32_t name_len = name.size();
  wire.append(reinterpret_cast<const char*>(&name_len), sizeof(uint32_t));
  wire.append(name);

  auto ff = proxy_wasm::getForeignFunction("stats_filter_set_name_overrides");
  ASSERT_TRUE(ff != nullptr);

  proxy_wasm::WasmBase wasm_base(nullptr, "", "", "",
                                 std::unordered_map<std::string, std::string>{}, {});
  auto result = ff(wasm_base, wire, [](size_t) -> void* { return nullptr; });
  EXPECT_EQ(result, proxy_wasm::WasmResult::Ok);

  ASSERT_EQ(ctx.name_overrides.size(), 1);
  EXPECT_EQ(ctx.name_overrides[0].type, 1);
  EXPECT_EQ(ctx.name_overrides[0].index, 0);
  EXPECT_EQ(ctx.name_overrides[0].new_name, "envoy.rq");

  setActiveContext(nullptr);
}

// -- Inject metrics foreign function test --

TEST(StatsFilterInjectMetricsTest, InjectCounterAndGauge) {
  StatsFilterContext ctx;
  setActiveContext(&ctx);

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

  // 1 counter: "custom.count" value=99, 0 tags
  appendU32(1);
  appendStr("custom.count");
  appendU64(99);
  appendU32(0);

  // 1 gauge: "custom.gauge" value=7, 1 tag {"k": "v"}
  appendU32(1);
  appendStr("custom.gauge");
  appendU64(7);
  appendU32(1);
  appendStr("k");
  appendStr("v");

  auto ff = proxy_wasm::getForeignFunction("stats_filter_inject_metrics");
  ASSERT_TRUE(ff != nullptr);

  proxy_wasm::WasmBase wasm_base(nullptr, "", "", "",
                                 std::unordered_map<std::string, std::string>{}, {});
  auto result = ff(wasm_base, wire, [](size_t) -> void* { return nullptr; });
  EXPECT_EQ(result, proxy_wasm::WasmResult::Ok);

  ASSERT_EQ(ctx.synthetic_counters.size(), 1);
  EXPECT_EQ(ctx.synthetic_counters[0].name, "custom.count");
  EXPECT_EQ(ctx.synthetic_counters[0].value, 99);

  ASSERT_EQ(ctx.synthetic_gauges.size(), 1);
  EXPECT_EQ(ctx.synthetic_gauges[0].name, "custom.gauge");
  EXPECT_EQ(ctx.synthetic_gauges[0].value, 7);
  ASSERT_EQ(ctx.synthetic_gauges[0].tags.size(), 1);
  EXPECT_EQ(ctx.synthetic_gauges[0].tags[0].name_, "k");
  EXPECT_EQ(ctx.synthetic_gauges[0].tags[0].value_, "v");

  setActiveContext(nullptr);
}

} // namespace
} // namespace WasmFilter
} // namespace StatSinks
} // namespace Extensions
} // namespace Envoy
