load(
    "//bazel:envoy_build_system.bzl",
    "envoy_package",
)
load(
    "//test/extensions:extensions_build_system.bzl",
    "envoy_extension_cc_test",
)

licenses(["notice"])  # Apache 2

envoy_package()

envoy_extension_cc_test(
    name = "wasm_filter_stat_sink_test",
    srcs = ["wasm_filter_stat_sink_test.cc"],
    extension_names = ["envoy.stat_sinks.wasm_filter"],
    deps = [
        "//source/extensions/stat_sinks/wasm_filter:wasm_filter_stat_sink_lib",
        "//test/mocks/stats:stats_mocks",
    ],
)
