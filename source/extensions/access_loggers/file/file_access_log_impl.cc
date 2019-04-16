#include "extensions/access_loggers/file/file_access_log_impl.h"

#include "common/http/header_map_impl.h"
#include "common/common/utility.h"

#include<vector>

namespace Envoy {
namespace Extensions {
namespace AccessLoggers {
namespace File {

FileAccessLog::FileAccessLog(const std::string& access_log_path, AccessLog::FilterPtr&& filter,
                             AccessLog::FormatterPtr&& formatter,
                             AccessLog::AccessLogManager& log_manager,
                             const std::vector<AccessLog::AccessLogMask>& log_line_masks)
    : filter_(std::move(filter)), formatter_(std::move(formatter)) {
  log_file_ = log_manager.createAccessLog(access_log_path);
  log_line_masks_ = log_line_masks;
}

void FileAccessLog::log(const Http::HeaderMap* request_headers,
                        const Http::HeaderMap* response_headers,
                        const Http::HeaderMap* response_trailers,
                        const StreamInfo::StreamInfo& stream_info) {
  static Http::HeaderMapImpl empty_headers;
  if (!request_headers) {
    request_headers = &empty_headers;
  }
  if (!response_headers) {
    response_headers = &empty_headers;
  }
  if (!response_trailers) {
    response_trailers = &empty_headers;
  }

  if (filter_) {
    if (!filter_->evaluate(stream_info, *request_headers, *response_headers, *response_trailers)) {
      return;
    }
  }

  std::string log_line = formatter_->format(*request_headers, *response_headers, *response_trailers, stream_info);

  for(AccessLog::AccessLogMask mask : log_line_masks_) {
    log_line = std::regex_replace(log_line, mask.regex_, mask.replace_with_);
  }
  
  log_file_->write(log_line);
}

} // namespace File
} // namespace AccessLoggers
} // namespace Extensions
} // namespace Envoy
