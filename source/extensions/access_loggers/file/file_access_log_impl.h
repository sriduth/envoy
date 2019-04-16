#pragma once

#include "envoy/access_log/access_log.h"
#include <vector>

namespace Envoy {
namespace Extensions {
namespace AccessLoggers {
namespace File {

/**
 * Access log Instance that writes logs to a file.
 */
class FileAccessLog : public AccessLog::Instance {
public:
  FileAccessLog(const std::string& access_log_path, AccessLog::FilterPtr&& filter,
                AccessLog::FormatterPtr&& formatter, AccessLog::AccessLogManager& log_manager,
                const std::vector<AccessLog::AccessLogMask>& log_line_masks = std::vector<AccessLog::AccessLogMask>());

  // AccessLog::Instance
  void log(const Http::HeaderMap* request_headers, const Http::HeaderMap* response_headers,
           const Http::HeaderMap* response_trailers,
           const StreamInfo::StreamInfo& stream_info) override;

private:
  AccessLog::AccessLogFileSharedPtr log_file_;
  AccessLog::FilterPtr filter_;
  AccessLog::FormatterPtr formatter_;
  std::vector<AccessLog::AccessLogMask> log_line_masks_;
};

} // namespace File
} // namespace AccessLoggers
} // namespace Extensions
} // namespace Envoy
