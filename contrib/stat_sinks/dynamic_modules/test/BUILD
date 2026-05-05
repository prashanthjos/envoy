load(
    "//bazel:envoy_build_system.bzl",
    "envoy_cc_test",
    "envoy_contrib_package",
)

licenses(["notice"])  # Apache 2

envoy_contrib_package()

envoy_cc_test(
    name = "config_test",
    srcs = ["config_test.cc"],
    data = [
        "//test/extensions/dynamic_modules/test_data/c:stat_sink_config_new_fail",
        "//test/extensions/dynamic_modules/test_data/c:stat_sink_missing_config_destroy",
        "//test/extensions/dynamic_modules/test_data/c:stat_sink_missing_config_new",
        "//test/extensions/dynamic_modules/test_data/c:stat_sink_missing_flush",
        "//test/extensions/dynamic_modules/test_data/c:stat_sink_missing_histogram_complete",
        "//test/extensions/dynamic_modules/test_data/c:stat_sink_no_op",
    ],
    deps = [
        "//contrib/stat_sinks/dynamic_modules/source:config_lib",
        "//envoy/registry",
        "//test/extensions/dynamic_modules:util",
        "//test/mocks/server:server_factory_context_mocks",
        "//test/test_common:utility_lib",
        "@envoy_api//contrib/envoy/extensions/stat_sinks/dynamic_modules/v3:pkg_cc_proto",
    ],
)

envoy_cc_test(
    name = "sink_test",
    srcs = ["sink_test.cc"],
    data = [
        "//test/extensions/dynamic_modules/test_data/c:stat_sink_config_new_fail",
        "//test/extensions/dynamic_modules/test_data/c:stat_sink_missing_config_new",
        "//test/extensions/dynamic_modules/test_data/c:stat_sink_no_op",
    ],
    deps = [
        "//contrib/stat_sinks/dynamic_modules/source:sink_config_lib",
        "//contrib/stat_sinks/dynamic_modules/source:sink_lib",
        "//source/extensions/dynamic_modules:dynamic_modules_lib",
        "//test/extensions/dynamic_modules:util",
        "//test/mocks/stats:stats_mocks",
        "//test/test_common:utility_lib",
    ],
)

envoy_cc_test(
    name = "abi_impl_test",
    srcs = ["abi_impl_test.cc"],
    deps = [
        "//contrib/stat_sinks/dynamic_modules/source:sink_lib",
        "//source/extensions/dynamic_modules/abi",
        "//test/mocks/stats:stats_mocks",
        "//test/test_common:utility_lib",
    ],
)

envoy_cc_test(
    name = "integration_test",
    srcs = ["integration_test.cc"],
    data = [
        "//test/extensions/dynamic_modules/test_data/c:stat_sink_integration_test",
    ],
    deps = [
        "//contrib/stat_sinks/dynamic_modules/source:config_lib",
        "//test/extensions/dynamic_modules:util",
        "//test/integration:http_integration_lib",
        "//test/test_common:environment_lib",
        "//test/test_common:utility_lib",
        "@envoy_api//contrib/envoy/extensions/stat_sinks/dynamic_modules/v3:pkg_cc_proto",
        "@envoy_api//envoy/config/bootstrap/v3:pkg_cc_proto",
    ],
)
