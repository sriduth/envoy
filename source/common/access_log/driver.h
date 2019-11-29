#ifndef DRIVER_HH
# define DRIVER_HH

#include <string>
#include <vector>

#include "absl/types/optional.h"
#include "common/access_log/location.hh"
#include "common/access_log/parser.hh"

# define YY_DECL \
  Envoy::AccessLog::Parser::symbol_type yylex(Envoy::AccessLog::Driver& drv)

namespace Envoy {
namespace AccessLog {

class Driver
{
public:
  Driver() {};

  ~Driver();
  
  int result;

  std::function<void(const std::string)> simple_expr_cb;

  std::function<void(const std::string)> plain_text_cb;
  
  std::function<void(const std::string,
		     const std::string,
		     absl::optional<std::string>,
		     absl::optional<std::string>)> replace_expr_cb;
  
  
  int parse (const std::string log_format);

  // Whether to generate parser debug traces.
  bool trace_parsing = false;

  void scan_begin (std::string log_format);
  void scan_end();

  // Whether to generate scanner debug traces.
  bool trace_scanning = false;
  // The token's location used by the scanner.
  Envoy::AccessLog::location location;

  void lookup_item_cb(const std::string function,
		      const std::string main_key,
		      const absl::optional<std::string> alt_key = absl::nullopt,
		      const absl::optional<std::string> length = absl::nullopt);
};
  
} // namespace AccessLog

} // namespace Envoy
// ... and declare it for the parser's sake.

Envoy::AccessLog::Parser::symbol_type yylex(Envoy::AccessLog::Driver& drv);

#endif // ! DRIVER_HH

