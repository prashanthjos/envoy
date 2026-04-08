load(
    "//bazel:envoy_build_system.bzl",
    "envoy_cc_test",
    "envoy_contrib_package",
)

licenses(["notice"])  # Apache 2

envoy_contrib_package()

envoy_cc_test(
    name = "wasm_filter_stat_sink_test",
    srcs = ["wasm_filter_stat_sink_test.cc"],
    deps = [
        "//contrib/stat_sinks/wasm_filter/source:wasm_filter_stat_sink_lib",
        "//test/mocks/stats:stats_mocks",
    ],
)

envoy_cc_test(
    name = "config_test",
    srcs = ["config_test.cc"],
    deps = [
        "//contrib/stat_sinks/wasm_filter/source:config_lib",
        "//test/mocks/server:server_mocks",
        "//test/test_common:utility_lib",
        "@envoy_api//contrib/envoy/extensions/stat_sinks/wasm_filter/v3:pkg_cc_proto",
    ],
)
