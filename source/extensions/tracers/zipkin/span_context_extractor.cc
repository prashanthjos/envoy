#include "source/extensions/tracers/zipkin/span_context_extractor.h"

#include "source/common/common/assert.h"
#include "source/common/common/utility.h"
#include "source/extensions/tracers/zipkin/span_context.h"
#include "source/extensions/tracers/zipkin/zipkin_core_constants.h"

namespace Envoy {
namespace Extensions {
namespace Tracers {
namespace Zipkin {
namespace {
constexpr int FormatMaxLength = 32 + 1 + 16 + 3 + 16; // traceid128-spanid-1-parentid
bool validSamplingFlags(char c) {
  if (c == '1' || c == '0' || c == 'd') {
    return true;
  }
  return false;
}

bool getSamplingFlags(char c, const Tracing::Decision tracing_decision) {
  if (validSamplingFlags(c)) {
    return c == '0' ? false : true;
  } else {
    return tracing_decision.traced;
  }
}

} // namespace

SpanContextExtractor::SpanContextExtractor(Tracing::TraceContext& trace_context,
                                           bool w3c_fallback_enabled)
    : trace_context_(trace_context), w3c_fallback_enabled_(w3c_fallback_enabled) {}

SpanContextExtractor::~SpanContextExtractor() = default;

bool SpanContextExtractor::extractSampled(const Tracing::Decision tracing_decision) {
  bool sampled(false);

  // Try B3 single format first
  auto b3_header_entry = ZipkinCoreConstants::get().B3.get(trace_context_);
  if (b3_header_entry.has_value()) {
    // This is an implicitly untrusted header, so only the first value is used.
    absl::string_view b3 = b3_header_entry.value();
    int sampled_pos = 0;
    switch (b3.length()) {
    case 1:
      break;
    case 35: // 16 + 1 + 16 + 2
      sampled_pos = 34;
      break;
    case 51: // 32 + 1 + 16 + 2
      sampled_pos = 50;
      break;
    case 52: // 16 + 1 + 16 + 2 + 1 + 16
      sampled_pos = 34;
      break;
    case 68: // 32 + 1 + 16 + 2 + 1 + 16
      sampled_pos = 50;
      break;
    default:
      return tracing_decision.traced;
    }
    return getSamplingFlags(b3[sampled_pos], tracing_decision);
  }

  // Try individual B3 sampled header
  auto x_b3_sampled_entry = ZipkinCoreConstants::get().X_B3_SAMPLED.get(trace_context_);
  if (x_b3_sampled_entry.has_value()) {
    // Checking if sampled flag has been specified. Also checking for 'true' value, as some old
    // zipkin tracers may still use that value, although should be 0 or 1.
    // This is an implicitly untrusted header, so only the first value is used.
    absl::string_view xb3_sampled = x_b3_sampled_entry.value();
    sampled = xb3_sampled == SAMPLED || xb3_sampled == "true";
    return sampled;
  }

  // Try W3C Trace Context format as fallback only if enabled
  if (w3c_fallback_enabled_) {
    Extensions::Tracers::OpenTelemetry::SpanContextExtractor w3c_extractor(
        const_cast<Tracing::TraceContext&>(trace_context_));
    if (w3c_extractor.propagationHeaderPresent()) {
      auto w3c_span_context = w3c_extractor.extractSpanContext();
      if (w3c_span_context.ok()) {
        return w3c_span_context.value().sampled();
      }
    }
  }

  return tracing_decision.traced;
}

std::pair<SpanContext, bool> SpanContextExtractor::extractSpanContext(bool is_sampled) {
  // Try B3 single format first
  if (ZipkinCoreConstants::get().B3.get(trace_context_).has_value()) {
    return extractSpanContextFromB3SingleFormat(is_sampled);
  }

  // Try individual B3 headers
  auto b3_trace_id_entry = ZipkinCoreConstants::get().X_B3_TRACE_ID.get(trace_context_);
  auto b3_span_id_entry = ZipkinCoreConstants::get().X_B3_SPAN_ID.get(trace_context_);
  if (b3_span_id_entry.has_value() && b3_trace_id_entry.has_value()) {
    uint64_t trace_id(0);
    uint64_t trace_id_high(0);
    uint64_t span_id(0);
    uint64_t parent_id(0);

    // Extract trace id - which can either be 128 or 64 bit. For 128 bit,
    // it needs to be divided into two 64 bit numbers (high and low).
    // This is an implicitly untrusted header, so only the first value is used.
    const std::string tid(b3_trace_id_entry.value());
    if (b3_trace_id_entry.value().size() == 32) {
      const std::string high_tid = tid.substr(0, 16);
      const std::string low_tid = tid.substr(16, 16);
      if (!StringUtil::atoull(high_tid.c_str(), trace_id_high, 16) ||
          !StringUtil::atoull(low_tid.c_str(), trace_id, 16)) {
        throw ExtractorException(
            fmt::format("Invalid traceid_high {} or tracid {}", high_tid.c_str(), low_tid.c_str()));
      }
    } else if (!StringUtil::atoull(tid.c_str(), trace_id, 16)) {
      throw ExtractorException(absl::StrCat("Invalid trace_id ", tid.c_str()));
    }

    // This is an implicitly untrusted header, so only the first value is used.
    const std::string spid(b3_span_id_entry.value());
    if (!StringUtil::atoull(spid.c_str(), span_id, 16)) {
      throw ExtractorException(absl::StrCat("Invalid span id ", spid.c_str()));
    }

    auto b3_parent_id_entry = ZipkinCoreConstants::get().X_B3_PARENT_SPAN_ID.get(trace_context_);
    if (b3_parent_id_entry.has_value() && !b3_parent_id_entry.value().empty()) {
      // This is an implicitly untrusted header, so only the first value is used.
      const std::string pspid(b3_parent_id_entry.value());
      if (!StringUtil::atoull(pspid.c_str(), parent_id, 16)) {
        throw ExtractorException(absl::StrCat("Invalid parent span id ", pspid.c_str()));
      }
    }

    return {SpanContext(trace_id_high, trace_id, span_id, parent_id, is_sampled), true};
  }

  // Try W3C Trace Context format as fallback only if enabled
  if (w3c_fallback_enabled_) {
    Extensions::Tracers::OpenTelemetry::SpanContextExtractor w3c_extractor(
        const_cast<Tracing::TraceContext&>(trace_context_));
    if (w3c_extractor.propagationHeaderPresent()) {
      auto w3c_span_context = w3c_extractor.extractSpanContext();
      if (w3c_span_context.ok()) {
        return convertW3CToZipkin(w3c_span_context.value(), is_sampled);
      }
    }
  }

  return {SpanContext(), false};
}

std::pair<SpanContext, bool>
SpanContextExtractor::extractSpanContextFromB3SingleFormat(bool is_sampled) {
  auto b3_head_entry = ZipkinCoreConstants::get().B3.get(trace_context_);
  ASSERT(b3_head_entry.has_value());
  // This is an implicitly untrusted header, so only the first value is used.
  const std::string b3(b3_head_entry.value());
  if (!b3.length()) {
    throw ExtractorException("Invalid input: empty");
  }

  if (b3.length() == 1) { // possibly sampling flags
    if (validSamplingFlags(b3[0])) {
      return {SpanContext(), false};
    }
    throw ExtractorException(fmt::format("Invalid input: invalid sampling flag {}", b3[0]));
  }

  if (b3.length() < 16 + 1 + 16 /* traceid64-spanid */) {
    throw ExtractorException("Invalid input: truncated");
  } else if (b3.length() > FormatMaxLength) {
    throw ExtractorException("Invalid input: too long");
  }

  uint64_t trace_id(0);
  uint64_t trace_id_high(0);
  uint64_t span_id(0);
  uint64_t parent_id(0);

  uint64_t pos = 0;

  const std::string trace_id_str = b3.substr(pos, 16);
  if (b3[pos + 32] == '-') {
    if (!StringUtil::atoull(trace_id_str.c_str(), trace_id_high, 16)) {
      throw ExtractorException(
          fmt::format("Invalid input: invalid trace id high {}", trace_id_str.c_str()));
    }
    pos += 16;
    const std::string trace_id_low_str = b3.substr(pos, 16);
    if (!StringUtil::atoull(trace_id_low_str.c_str(), trace_id, 16)) {
      throw ExtractorException(
          fmt::format("Invalid input: invalid trace id {}", trace_id_low_str.c_str()));
    }
  } else {
    if (!StringUtil::atoull(trace_id_str.c_str(), trace_id, 16)) {
      throw ExtractorException(
          fmt::format("Invalid input: invalid trace id {}", trace_id_str.c_str()));
    }
  }

  pos += 16; // traceId ended
  if (!(b3[pos++] == '-')) {
    throw ExtractorException("Invalid input: not exists span id");
  }

  const std::string span_id_str = b3.substr(pos, 16);
  if (!StringUtil::atoull(span_id_str.c_str(), span_id, 16)) {
    throw ExtractorException(fmt::format("Invalid input: invalid span id {}", span_id_str.c_str()));
  }
  pos += 16; // spanId ended

  if (b3.length() > pos) {
    // If we are at this point, we have more than just traceId-spanId.
    // If the sampling field is present, we'll have a delimiter 2 characters from now. Ex "-1"
    // If it is absent, but a parent ID is (which is strange), we'll have at least 17 characters.
    // Therefore, if we have less than two characters, the input is truncated.
    if (b3.length() == (pos + 1)) {
      throw ExtractorException("Invalid input: truncated");
    }

    if (!(b3[pos++] == '-')) {
      throw ExtractorException("Invalid input: not exists sampling field");
    }

    // If our position is at the end of the string, or another delimiter is one character past our
    // position, try to read sampled status.
    if (b3.length() == pos + 1 || ((b3.length() >= pos + 2) && (b3[pos + 1] == '-'))) {
      if (!validSamplingFlags(b3[pos])) {
        throw ExtractorException(fmt::format("Invalid input: invalid sampling flag {}", b3[pos]));
      }
      pos++; // consume the sampled status
    } else {
      throw ExtractorException("Invalid input: truncated");
    }

    if (b3.length() > pos) {
      // If we are at this point, we should have a parent ID, encoded as "-[0-9a-f]{16}"
      if (b3.length() != pos + 17) {
        throw ExtractorException("Invalid input: truncated");
      }

      ASSERT(b3[pos] == '-');
      pos++;

      const std::string parent_id_str = b3.substr(pos, b3.length() - pos);
      if (!StringUtil::atoull(parent_id_str.c_str(), parent_id, 16)) {
        throw ExtractorException(
            fmt::format("Invalid input: invalid parent id {}", parent_id_str.c_str()));
      }
    }
  }

  return {SpanContext(trace_id_high, trace_id, span_id, parent_id, is_sampled), true};
}

std::pair<SpanContext, bool> SpanContextExtractor::convertW3CToZipkin(
    const Extensions::Tracers::OpenTelemetry::SpanContext& w3c_context, bool fallback_sampled) {
  // Convert W3C 128-bit trace ID (32 hex chars) to Zipkin format
  const std::string& trace_id_str = w3c_context.traceId();

  if (trace_id_str.length() != 32) {
    throw ExtractorException(fmt::format("Invalid W3C trace ID length: {}", trace_id_str.length()));
  }

  // Split 128-bit trace ID into high and low 64-bit parts for Zipkin
  const std::string trace_id_high_str = trace_id_str.substr(0, 16);
  const std::string trace_id_low_str = trace_id_str.substr(16, 16);

  uint64_t trace_id_high(0);
  uint64_t trace_id(0);
  if (!StringUtil::atoull(trace_id_high_str.c_str(), trace_id_high, 16) ||
      !StringUtil::atoull(trace_id_low_str.c_str(), trace_id, 16)) {
    throw ExtractorException(fmt::format("Invalid W3C trace ID: {}", trace_id_str));
  }

  // Convert W3C parent ID (16 hex chars) to Zipkin span ID
  const std::string& parent_id_str = w3c_context.parentId();
  if (parent_id_str.length() != 16) {
    throw ExtractorException(
        fmt::format("Invalid W3C parent ID length: {}", parent_id_str.length()));
  }

  uint64_t span_id(0);
  if (!StringUtil::atoull(parent_id_str.c_str(), span_id, 16)) {
    throw ExtractorException(fmt::format("Invalid W3C parent ID: {}", parent_id_str));
  }

  // W3C doesn't have a direct parent span concept like B3
  // The W3C parent-id becomes our span-id, and we don't set a parent
  uint64_t parent_id(0);

  // Use W3C sampling decision, or fallback if not specified
  bool sampled = w3c_context.sampled() || fallback_sampled;

  return {SpanContext(trace_id_high, trace_id, span_id, parent_id, sampled), true};
}

} // namespace Zipkin
} // namespace Tracers
} // namespace Extensions
} // namespace Envoy
