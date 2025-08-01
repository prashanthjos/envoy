load(
    "//bazel:envoy_build_system.bzl",
    "envoy_cc_extension",
    "envoy_cc_library",
    "envoy_extension_package",
)

licenses(["notice"])  # Apache 2

# Trace driver for Zipkin (https://zipkin.io/).

envoy_extension_package()

envoy_cc_library(
    name = "zipkin_lib",
    srcs = [
        "span_buffer.cc",
        "span_context_extractor.cc",
        "tracer.cc",
        "util.cc",
        "zipkin_core_types.cc",
        "zipkin_tracer_impl.cc",
    ],
    hdrs = [
        "span_buffer.h",
        "span_context.h",
        "span_context_extractor.h",
        "tracer.h",
        "tracer_interface.h",
        "util.h",
        "zipkin_core_constants.h",
        "zipkin_core_types.h",
        "zipkin_json_field_names.h",
        "zipkin_tracer_impl.h",
    ],
    deps = [
        "//envoy/common:time_interface",
        "//envoy/local_info:local_info_interface",
        "//envoy/network:address_interface",
        "//envoy/runtime:runtime_interface",
        "//envoy/thread_local:thread_local_interface",
        "//envoy/tracing:tracer_interface",
        "//envoy/upstream:cluster_manager_interface",
        "//source/common/common:enum_to_int",
        "//source/common/common:hex_lib",
        "//source/common/common:utility_lib",
        "//source/common/config:utility_lib",
        "//source/common/http:async_client_utility_lib",
        "//source/common/http:header_map_lib",
        "//source/common/http:message_lib",
        "//source/common/http:utility_lib",
        "//source/common/json:json_loader_lib",
        "//source/common/network:address_lib",
        "//source/common/singleton:const_singleton",
        "//source/common/tracing:http_tracer_lib",
        "//source/common/upstream:cluster_update_tracker_lib",
        "//source/extensions/tracers/opentelemetry:opentelemetry_tracer_lib",
        "@com_github_openzipkin_zipkinapi//:zipkin_cc_proto",
        "@com_google_absl//absl/types:optional",
        "@envoy_api//envoy/config/trace/v3:pkg_cc_proto",
    ],
)

envoy_cc_extension(
    name = "config",
    srcs = ["config.cc"],
    hdrs = ["config.h"],
    deps = [
        ":zipkin_lib",
        "//source/extensions/tracers/common:factory_base_lib",
        "@envoy_api//envoy/config/trace/v3:pkg_cc_proto",
    ],
)
