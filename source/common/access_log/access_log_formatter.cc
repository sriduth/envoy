#include "common/access_log/access_log_formatter.h"

#include <cstdint>
#include <regex>
#include <string>
#include <vector>

#include "common/common/assert.h"
#include "common/common/fmt.h"
#include "common/common/utility.h"
#include "common/config/metadata.h"
#include "common/http/utility.h"
#include "common/stream_info/utility.h"

#include "source/common/access_log/access_log_grammar.inc/access_log_grammar/AccessLogLexer.h"
#include "source/common/access_log/access_log_grammar.inc/access_log_grammar/AccessLogParser.h"
#include "common/access_log/driver.h"
#include "antlr4-runtime.h"

#include "absl/strings/str_split.h"
#include "fmt/format.h"

using Envoy::Config::Metadata;

namespace Envoy {
namespace AccessLog {

static const std::string UnspecifiedValueString = "-";

namespace {

const std::regex& getStartTimeNewlinePattern() {
  CONSTRUCT_ON_FIRST_USE(std::regex, "%[-_0^#]*[1-9]*n");
}

// Helper that handles the case when the ConnectionInfo is missing or if the desired value is
// empty.
StreamInfoFormatter::FieldExtractor sslConnectionInfoStringExtractor(
    std::function<std::string(const Ssl::ConnectionInfo& connection_info)> string_extractor) {
  return [string_extractor](const StreamInfo::StreamInfo& stream_info) {
    if (stream_info.downstreamSslConnection() == nullptr) {
      return UnspecifiedValueString;
    }

    const auto value = string_extractor(*stream_info.downstreamSslConnection());
    if (value.empty()) {
      return UnspecifiedValueString;
    } else {
      return value;
    }
  };
}

// Helper that handles the case when the desired time field is empty.
StreamInfoFormatter::FieldExtractor sslConnectionInfoStringTimeExtractor(
    std::function<absl::optional<SystemTime>(const Ssl::ConnectionInfo& connection_info)>
        time_extractor) {
  return sslConnectionInfoStringExtractor(
      [time_extractor](const Ssl::ConnectionInfo& connection_info) {
        absl::optional<SystemTime> time = time_extractor(connection_info);
        if (!time.has_value()) {
          return UnspecifiedValueString;
        }

        return AccessLogDateTimeFormatter::fromTime(time.value());
      });
}

} // namespace

const std::string AccessLogFormatUtils::DEFAULT_FORMAT =
    "[%START_TIME%] \"%REQ(:METHOD)% %REQ(X-ENVOY-ORIGINAL-PATH?:PATH)% %PROTOCOL%\" "
    "%RESPONSE_CODE% %RESPONSE_FLAGS% %BYTES_RECEIVED% %BYTES_SENT% %DURATION% "
    "%RESP(X-ENVOY-UPSTREAM-SERVICE-TIME)% "
    "\"%REQ(X-FORWARDED-FOR)%\" \"%REQ(USER-AGENT)%\" \"%REQ(X-REQUEST-ID)%\" "
    "\"%REQ(:AUTHORITY)%\" \"%UPSTREAM_HOST%\"\n";

FormatterPtr AccessLogFormatUtils::defaultAccessLogFormatter() {
  return FormatterPtr{new FormatterImpl(DEFAULT_FORMAT)};
}

std::string
AccessLogFormatUtils::durationToString(const absl::optional<std::chrono::nanoseconds>& time) {
  if (time) {
    return durationToString(time.value());
  } else {
    return UnspecifiedValueString;
  }
}

std::string AccessLogFormatUtils::durationToString(const std::chrono::nanoseconds& time) {
  return fmt::format_int(std::chrono::duration_cast<std::chrono::milliseconds>(time).count()).str();
}

const std::string&
AccessLogFormatUtils::protocolToString(const absl::optional<Http::Protocol>& protocol) {
  if (protocol) {
    return Http::Utility::getProtocolString(protocol.value());
  }
  return UnspecifiedValueString;
}

FormatterImpl::FormatterImpl(const std::string& format) {
  providers_ = AccessLogFormatParser::parse(format);
}

std::string FormatterImpl::format(const Http::HeaderMap& request_headers,
                                  const Http::HeaderMap& response_headers,
                                  const Http::HeaderMap& response_trailers,
                                  const StreamInfo::StreamInfo& stream_info) const {
  std::string log_line;
  log_line.reserve(256);

  for (const FormatterProviderPtr& provider : providers_) {
    log_line += provider->format(request_headers, response_headers, response_trailers, stream_info);
  }

  return log_line;
}

JsonFormatterImpl::JsonFormatterImpl(std::unordered_map<std::string, std::string>& format_mapping) {
  for (const auto& pair : format_mapping) {
    auto providers = AccessLogFormatParser::parse(pair.second);
    json_output_format_.emplace(pair.first, FormatterPtr{new FormatterImpl(pair.second)});
  }
}

std::string JsonFormatterImpl::format(const Http::HeaderMap& request_headers,
                                      const Http::HeaderMap& response_headers,
                                      const Http::HeaderMap& response_trailers,
                                      const StreamInfo::StreamInfo& stream_info) const {
  const auto output_map = toMap(request_headers, response_headers, response_trailers, stream_info);

  ProtobufWkt::Struct output_struct;
  for (const auto& pair : output_map) {
    ProtobufWkt::Value string_value;
    string_value.set_string_value(pair.second);
    (*output_struct.mutable_fields())[pair.first] = string_value;
  }

  std::string log_line;
  const auto conversion_status = Protobuf::util::MessageToJsonString(output_struct, &log_line);
  if (!conversion_status.ok()) {
    log_line =
        fmt::format("Error serializing access log to JSON: {}", conversion_status.ToString());
  }

  return absl::StrCat(log_line, "\n");
}

std::unordered_map<std::string, std::string> JsonFormatterImpl::toMap(
    const Http::HeaderMap& request_headers, const Http::HeaderMap& response_headers,
    const Http::HeaderMap& response_trailers, const StreamInfo::StreamInfo& stream_info) const {
  std::unordered_map<std::string, std::string> output;
  for (const auto& pair : json_output_format_) {
    output.emplace(pair.first, pair.second->format(request_headers, response_headers,
                                                   response_trailers, stream_info));
  }
  return output;
}


// TODO(derekargueta): #2967 - Rewrite AccessLogFormatter with parser library & formal grammar
std::vector<FormatterProviderPtr> AccessLogFormatParser::parse(const std::string& log_fmt) {
  std::vector<FormatterProviderPtr> formatters;

  auto replace_cb = [&formatters](const std::string function,
  				  const std::string key,
  				  absl::optional<std::string> alternate_,
  				  absl::optional<std::string> size) -> void
  		    {
  		      absl::optional<size_t> max_length = absl::nullopt;
		      
  		      std::string alternate = "";
		      
  		      if(alternate_ != absl::nullopt) {
  			alternate = alternate_.value();
  		      }
		      
  		      if(size != absl::nullopt) {
  			uint64_t length_value;
  			std::string length_str = size.value();
  			if (!absl::SimpleAtoi(length_str, &length_value)) {
  			  throw EnvoyException(fmt::format("Length must be an integer, given: {}",
  							   length_str));
  			}

  			max_length = length_value;
  		      }

  		      if(function == "REQ"){
  			formatters.emplace_back(FormatterProviderPtr{
  			    new RequestHeaderFormatter(key, alternate, max_length)});
  		      } else if(function == "RESP") {
  			formatters.emplace_back(FormatterProviderPtr{
  			    new ResponseHeaderFormatter(key, alternate, max_length)});
  		      } else if(function == "TRAILER"){
  			formatters.emplace_back(FormatterProviderPtr{
  			    new ResponseTrailerFormatter(key, alternate, max_length)});    
  		      } else if(function == "DYNAMIC_METADATA") {
  			std::vector<std::string> key_parts = absl::StrSplit(key, ":");
  			std::string ns = key_parts[0];
  			std::vector<std::string> path(key_parts.begin() + 1, key_parts.end());
			
  			formatters.emplace_back(FormatterProviderPtr{
  			    new DynamicMetadataFormatter(ns, path, max_length)});
  		      } else if(function == "FILTER_STATE") {
  			formatters.emplace_back(std::make_unique<FilterStateFormatter>
  						(key, max_length));			
  		      } else if(function == "START_TIME") {
  			if (std::regex_search(key, getStartTimeNewlinePattern())) {
  			  throw EnvoyException("Invalid header configuration. Format string contains newline.");
  			}
  			formatters.emplace_back(FormatterProviderPtr{
  			    new StartTimeFormatter(key) });
  		      } else {
  			formatters.emplace_back(FormatterProviderPtr{
  			    new StreamInfoFormatter(function)});	
  		      }
  		    };
  
  auto plain_text_cb = [&formatters](const std::string plain_string) -> void {
  			 formatters.emplace_back(FormatterProviderPtr{
  			    new PlainStringFormatter(plain_string)});	
  		       };
  
  auto simple_cb = [&formatters](const std::string replace) -> void {
  		     if(replace == "START_TIME") {
  		       formatters.emplace_back(FormatterProviderPtr{
  			   new StartTimeFormatter("") });
  		     } else {
  		       formatters.emplace_back(FormatterProviderPtr{
  			   new StreamInfoFormatter(replace)});
  		     }
  		   };

  antlr4::ANTLRInputStream input(log_fmt);
  ::access_log_grammar::AccessLogLexer lexer(&input);

  antlr4::CommonTokenStream tokens(&lexer);

  ::access_log_grammar::AccessLogParser parser(&tokens);

  Driver d;
  d.simple_expr_cb = simple_cb;
  d.plain_text_cb = plain_text_cb;
  d.replace_expr_cb = replace_cb;

  parser.addErrorListener(&d);

  antlr4::tree::ParseTree *tree = parser.start();

  try
  {
    antlr4::tree::ParseTreeWalker::DEFAULT.walk(&d, tree);
  }
  catch(const std::exception&)
  {
      d.parse_success = false;
  }

  if(!d.parse_success) {
    throw EnvoyException(d.error_message.value_or("Could not parse"));
  }
  
  return formatters;
}

StreamInfoFormatter::StreamInfoFormatter(const std::string& field_name) {

  if (field_name == "REQUEST_DURATION") {
    field_extractor_ = [](const StreamInfo::StreamInfo& stream_info) {
      return AccessLogFormatUtils::durationToString(stream_info.lastDownstreamRxByteReceived());
    };
  } else if (field_name == "RESPONSE_DURATION") {
    field_extractor_ = [](const StreamInfo::StreamInfo& stream_info) {
      return AccessLogFormatUtils::durationToString(stream_info.firstUpstreamRxByteReceived());
    };
  } else if (field_name == "RESPONSE_TX_DURATION") {
    field_extractor_ = [](const StreamInfo::StreamInfo& stream_info) {
      auto downstream = stream_info.lastDownstreamTxByteSent();
      auto upstream = stream_info.firstUpstreamRxByteReceived();

      if (downstream && upstream) {
        auto val = downstream.value() - upstream.value();
        return AccessLogFormatUtils::durationToString(val);
      }

      return UnspecifiedValueString;
    };
  } else if (field_name == "BYTES_RECEIVED") {
    field_extractor_ = [](const StreamInfo::StreamInfo& stream_info) {
      return fmt::format_int(stream_info.bytesReceived()).str();
    };
  } else if (field_name == "PROTOCOL") {
    field_extractor_ = [](const StreamInfo::StreamInfo& stream_info) {
      return AccessLogFormatUtils::protocolToString(stream_info.protocol());
    };
  } else if (field_name == "RESPONSE_CODE") {
    field_extractor_ = [](const StreamInfo::StreamInfo& stream_info) {
      return stream_info.responseCode() ? fmt::format_int(stream_info.responseCode().value()).str()
                                        : "0";
    };
  } else if (field_name == "RESPONSE_CODE_DETAILS") {
    field_extractor_ = [](const StreamInfo::StreamInfo& stream_info) {
      return stream_info.responseCodeDetails() ? stream_info.responseCodeDetails().value()
                                               : UnspecifiedValueString;
    };
  } else if (field_name == "BYTES_SENT") {
    field_extractor_ = [](const StreamInfo::StreamInfo& stream_info) {
      return fmt::format_int(stream_info.bytesSent()).str();
    };
  } else if (field_name == "DURATION") {
    field_extractor_ = [](const StreamInfo::StreamInfo& stream_info) {
      return AccessLogFormatUtils::durationToString(stream_info.requestComplete());
    };
  } else if (field_name == "RESPONSE_FLAGS") {
    field_extractor_ = [](const StreamInfo::StreamInfo& stream_info) {
      return StreamInfo::ResponseFlagUtils::toShortString(stream_info);
    };
  } else if (field_name == "UPSTREAM_HOST") {
    field_extractor_ = [](const StreamInfo::StreamInfo& stream_info) {
      if (stream_info.upstreamHost()) {
        return stream_info.upstreamHost()->address()->asString();
      } else {
        return UnspecifiedValueString;
      }
    };
  } else if (field_name == "UPSTREAM_CLUSTER") {
    field_extractor_ = [](const StreamInfo::StreamInfo& stream_info) {
      std::string upstream_cluster_name;
      if (nullptr != stream_info.upstreamHost()) {
        upstream_cluster_name = stream_info.upstreamHost()->cluster().name();
      }

      return upstream_cluster_name.empty() ? UnspecifiedValueString : upstream_cluster_name;
    };
  } else if (field_name == "UPSTREAM_LOCAL_ADDRESS") {
    field_extractor_ = [](const StreamInfo::StreamInfo& stream_info) {
      return stream_info.upstreamLocalAddress() != nullptr
                 ? stream_info.upstreamLocalAddress()->asString()
                 : UnspecifiedValueString;
    };
  } else if (field_name == "DOWNSTREAM_LOCAL_ADDRESS") {
    field_extractor_ = [](const StreamInfo::StreamInfo& stream_info) {
      return stream_info.downstreamLocalAddress()->asString();
    };
  } else if (field_name == "DOWNSTREAM_LOCAL_ADDRESS_WITHOUT_PORT") {
    field_extractor_ = [](const Envoy::StreamInfo::StreamInfo& stream_info) {
      return StreamInfo::Utility::formatDownstreamAddressNoPort(
          *stream_info.downstreamLocalAddress());
    };
  } else if (field_name == "DOWNSTREAM_REMOTE_ADDRESS") {
    field_extractor_ = [](const StreamInfo::StreamInfo& stream_info) {
      return stream_info.downstreamRemoteAddress()->asString();
    };
  } else if (field_name == "DOWNSTREAM_REMOTE_ADDRESS_WITHOUT_PORT") {
    field_extractor_ = [](const StreamInfo::StreamInfo& stream_info) {
      return StreamInfo::Utility::formatDownstreamAddressNoPort(
          *stream_info.downstreamRemoteAddress());
    };
  } else if (field_name == "DOWNSTREAM_DIRECT_REMOTE_ADDRESS") {
    field_extractor_ = [](const StreamInfo::StreamInfo& stream_info) {
      return stream_info.downstreamDirectRemoteAddress()->asString();
    };
  } else if (field_name == "DOWNSTREAM_DIRECT_REMOTE_ADDRESS_WITHOUT_PORT") {
    field_extractor_ = [](const StreamInfo::StreamInfo& stream_info) {
      return StreamInfo::Utility::formatDownstreamAddressNoPort(
          *stream_info.downstreamDirectRemoteAddress());
    };
  } else if (field_name == "REQUESTED_SERVER_NAME") {
    field_extractor_ = [](const StreamInfo::StreamInfo& stream_info) {
      if (!stream_info.requestedServerName().empty()) {
        return stream_info.requestedServerName();
      } else {
        return UnspecifiedValueString;
      }
    };
  } else if (field_name == "ROUTE_NAME") {
    field_extractor_ = [](const StreamInfo::StreamInfo& stream_info) {
      std::string route_name = stream_info.getRouteName();
      return route_name.empty() ? UnspecifiedValueString : route_name;
    };
  } else if (field_name == "DOWNSTREAM_PEER_URI_SAN") {
    field_extractor_ =
        sslConnectionInfoStringExtractor([](const Ssl::ConnectionInfo& connection_info) {
          return absl::StrJoin(connection_info.uriSanPeerCertificate(), ",");
        });
  } else if (field_name == "DOWNSTREAM_LOCAL_URI_SAN") {
    field_extractor_ =
        sslConnectionInfoStringExtractor([](const Ssl::ConnectionInfo& connection_info) {
          return absl::StrJoin(connection_info.uriSanLocalCertificate(), ",");
        });
  } else if (field_name == "DOWNSTREAM_PEER_SUBJECT") {
    field_extractor_ =
        sslConnectionInfoStringExtractor([](const Ssl::ConnectionInfo& connection_info) {
          return connection_info.subjectPeerCertificate();
        });
  } else if (field_name == "DOWNSTREAM_LOCAL_SUBJECT") {
    field_extractor_ =
        sslConnectionInfoStringExtractor([](const Ssl::ConnectionInfo& connection_info) {
          return connection_info.subjectLocalCertificate();
        });
  } else if (field_name == "DOWNSTREAM_TLS_SESSION_ID") {
    field_extractor_ = sslConnectionInfoStringExtractor(
        [](const Ssl::ConnectionInfo& connection_info) { return connection_info.sessionId(); });
  } else if (field_name == "DOWNSTREAM_TLS_CIPHER") {
    field_extractor_ =
        sslConnectionInfoStringExtractor([](const Ssl::ConnectionInfo& connection_info) {
          return connection_info.ciphersuiteString();
        });
  } else if (field_name == "DOWNSTREAM_TLS_VERSION") {
    field_extractor_ = sslConnectionInfoStringExtractor(
        [](const Ssl::ConnectionInfo& connection_info) { return connection_info.tlsVersion(); });
  } else if (field_name == "DOWNSTREAM_PEER_FINGERPRINT_256") {
    field_extractor_ =
        sslConnectionInfoStringExtractor([](const Ssl::ConnectionInfo& connection_info) {
          return connection_info.sha256PeerCertificateDigest();
        });
  } else if (field_name == "DOWNSTREAM_PEER_SERIAL") {
    field_extractor_ =
        sslConnectionInfoStringExtractor([](const Ssl::ConnectionInfo& connection_info) {
          return connection_info.serialNumberPeerCertificate();
        });
  } else if (field_name == "DOWNSTREAM_PEER_ISSUER") {
    field_extractor_ =
        sslConnectionInfoStringExtractor([](const Ssl::ConnectionInfo& connection_info) {
          return connection_info.issuerPeerCertificate();
        });
  } else if (field_name == "DOWNSTREAM_PEER_SUBJECT") {
    field_extractor_ =
        sslConnectionInfoStringExtractor([](const Ssl::ConnectionInfo& connection_info) {
          return connection_info.subjectPeerCertificate();
        });
  } else if (field_name == "DOWNSTREAM_PEER_CERT") {
    field_extractor_ =
        sslConnectionInfoStringExtractor([](const Ssl::ConnectionInfo& connection_info) {
          return connection_info.urlEncodedPemEncodedPeerCertificate();
        });
  } else if (field_name == "DOWNSTREAM_PEER_CERT_V_START") {
    field_extractor_ =
        sslConnectionInfoStringTimeExtractor([](const Ssl::ConnectionInfo& connection_info) {
          return connection_info.validFromPeerCertificate();
        });
  } else if (field_name == "DOWNSTREAM_PEER_CERT_V_END") {
    field_extractor_ =
        sslConnectionInfoStringTimeExtractor([](const Ssl::ConnectionInfo& connection_info) {
          return connection_info.expirationPeerCertificate();
        });
  } else if (field_name == "UPSTREAM_TRANSPORT_FAILURE_REASON") {
    field_extractor_ = [](const StreamInfo::StreamInfo& stream_info) {
      if (!stream_info.upstreamTransportFailureReason().empty()) {
        return stream_info.upstreamTransportFailureReason();
      } else {
        return UnspecifiedValueString;
      }
    };
  } else {
    throw EnvoyException(fmt::format("Not supported field in StreamInfo: {}", field_name));
  }
}

std::string StreamInfoFormatter::format(const Http::HeaderMap&, const Http::HeaderMap&,
                                        const Http::HeaderMap&,
                                        const StreamInfo::StreamInfo& stream_info) const {
  return field_extractor_(stream_info);
}

PlainStringFormatter::PlainStringFormatter(const std::string& str) : str_(str) {}

std::string PlainStringFormatter::format(const Http::HeaderMap&, const Http::HeaderMap&,
                                         const Http::HeaderMap&,
                                         const StreamInfo::StreamInfo&) const {
  return str_;
}

HeaderFormatter::HeaderFormatter(const std::string& main_header,
                                 const std::string& alternative_header,
                                 absl::optional<size_t> max_length)
    : main_header_(main_header), alternative_header_(alternative_header), max_length_(max_length) {}

std::string HeaderFormatter::format(const Http::HeaderMap& headers) const {
  const Http::HeaderEntry* header = headers.get(main_header_);

  if (!header && !alternative_header_.get().empty()) {
    header = headers.get(alternative_header_);
  }

  std::string header_value_string;
  if (!header) {
    header_value_string = UnspecifiedValueString;
  } else {
    header_value_string = std::string(header->value().getStringView());
  }

  if (max_length_ && header_value_string.length() > max_length_.value()) {
    return header_value_string.substr(0, max_length_.value());
  }

  return header_value_string;
}

ResponseHeaderFormatter::ResponseHeaderFormatter(const std::string& main_header,
                                                 const std::string& alternative_header,
                                                 absl::optional<size_t> max_length)
    : HeaderFormatter(main_header, alternative_header, max_length) {}

std::string ResponseHeaderFormatter::format(const Http::HeaderMap&,
                                            const Http::HeaderMap& response_headers,
                                            const Http::HeaderMap&,
                                            const StreamInfo::StreamInfo&) const {
  return HeaderFormatter::format(response_headers);
}

RequestHeaderFormatter::RequestHeaderFormatter(const std::string& main_header,
                                               const std::string& alternative_header,
                                               absl::optional<size_t> max_length)
    : HeaderFormatter(main_header, alternative_header, max_length) {}

std::string RequestHeaderFormatter::format(const Http::HeaderMap& request_headers,
                                           const Http::HeaderMap&, const Http::HeaderMap&,
                                           const StreamInfo::StreamInfo&) const {
  return HeaderFormatter::format(request_headers);
}

ResponseTrailerFormatter::ResponseTrailerFormatter(const std::string& main_header,
                                                   const std::string& alternative_header,
                                                   absl::optional<size_t> max_length)
    : HeaderFormatter(main_header, alternative_header, max_length) {}

std::string ResponseTrailerFormatter::format(const Http::HeaderMap&, const Http::HeaderMap&,
                                             const Http::HeaderMap& response_trailers,
                                             const StreamInfo::StreamInfo&) const {
  return HeaderFormatter::format(response_trailers);
}

MetadataFormatter::MetadataFormatter(const std::string& filter_namespace,
                                     const std::vector<std::string>& path,
                                     absl::optional<size_t> max_length)
    : filter_namespace_(filter_namespace), path_(path), max_length_(max_length) {}

std::string MetadataFormatter::format(const envoy::api::v2::core::Metadata& metadata) const {
  const Protobuf::Message* data;
  if (path_.empty()) {
    const auto filter_it = metadata.filter_metadata().find(filter_namespace_);
    if (filter_it == metadata.filter_metadata().end()) {
      return UnspecifiedValueString;
    }
    data = &(filter_it->second);
  } else {
    const ProtobufWkt::Value& val = Metadata::metadataValue(metadata, filter_namespace_, path_);
    if (val.kind_case() == ProtobufWkt::Value::KindCase::KIND_NOT_SET) {
      return UnspecifiedValueString;
    }
    data = &val;
  }
  std::string json;
  const auto status = Protobuf::util::MessageToJsonString(*data, &json);
  RELEASE_ASSERT(status.ok(), "");
  if (max_length_ && json.length() > max_length_.value()) {
    return json.substr(0, max_length_.value());
  }
  return json;
}

// TODO(glicht): Consider adding support for route/listener/cluster metadata as suggested by @htuch.
// See: https://github.com/envoyproxy/envoy/issues/3006
DynamicMetadataFormatter::DynamicMetadataFormatter(const std::string& filter_namespace,
                                                   const std::vector<std::string>& path,
                                                   absl::optional<size_t> max_length)
    : MetadataFormatter(filter_namespace, path, max_length) {}

std::string DynamicMetadataFormatter::format(const Http::HeaderMap&, const Http::HeaderMap&,
                                             const Http::HeaderMap&,
                                             const StreamInfo::StreamInfo& stream_info) const {
  return MetadataFormatter::format(stream_info.dynamicMetadata());
}

FilterStateFormatter::FilterStateFormatter(const std::string& key,
                                           absl::optional<size_t> max_length)
    : key_(key), max_length_(max_length) {}

std::string FilterStateFormatter::format(const Http::HeaderMap&, const Http::HeaderMap&,
                                         const Http::HeaderMap&,
                                         const StreamInfo::StreamInfo& stream_info) const {
  const StreamInfo::FilterState& filter_state = stream_info.filterState();
  if (!filter_state.hasDataWithName(key_)) {
    return UnspecifiedValueString;
  }

  const auto& object = filter_state.getDataReadOnly<StreamInfo::FilterState::Object>(key_);
  ProtobufTypes::MessagePtr proto = object.serializeAsProto();
  if (proto == nullptr) {
    return UnspecifiedValueString;
  }

  std::string value;
  const auto status = Protobuf::util::MessageToJsonString(*proto, &value);
  if (!status.ok()) {
    // If the message contains an unknown Any (from WASM or Lua), MessageToJsonString will fail.
    // TODO(lizan): add support of unknown Any.
    return UnspecifiedValueString;
  }
  if (max_length_.has_value() && value.length() > max_length_.value()) {
    return value.substr(0, max_length_.value());
  }
  return value;
}

StartTimeFormatter::StartTimeFormatter(const std::string& format) : date_formatter_(format) {}

std::string StartTimeFormatter::format(const Http::HeaderMap&, const Http::HeaderMap&,
                                       const Http::HeaderMap&,
                                       const StreamInfo::StreamInfo& stream_info) const {
  if (date_formatter_.formatString().empty()) {
    return AccessLogDateTimeFormatter::fromTime(stream_info.startTime());
  } else {
    return date_formatter_.fromTime(stream_info.startTime());
  }
}

} // namespace AccessLog
} // namespace Envoy
