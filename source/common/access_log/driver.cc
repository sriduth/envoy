#include "absl/types/optional.h"
#include "common/access_log/driver.h"
#include "common/access_log/parser.hh"


int Envoy::AccessLog::Driver::parse(const std::string format_string)
{
  try {
    scan_begin(format_string);
    Envoy::AccessLog::Parser parse (*this);
    parse.set_debug_level(trace_parsing);
    result = -1;
    result = parse();
  } catch (std::exception const & ex) {
    result = -1;
  }

  return result;
}


Envoy::AccessLog::Driver::~Driver() {
  scan_end();
}
