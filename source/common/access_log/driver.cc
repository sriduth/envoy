#include "absl/types/optional.h"
#include "common/access_log/driver.h"
#include "common/access_log/parser.hh"


int Envoy::AccessLog::Driver::parse(const std::string f)
{
  scan_begin(f);
  yy::parser parse (*this);
  parse.set_debug_level(trace_parsing);
  int res = parse();
  scan_end ();
  return res;
}
