#include <string>
#include <vector>

#include "absl/strings/str_split.h"
#include "absl/types/optional.h"
#include "./source/common/access_log/access_log_grammar.inc/access_log_grammar/AccessLogParserBaseListener.h"
#include "ANTLRErrorListener.h"

namespace Envoy {
namespace AccessLog {

class Driver : public ::access_log_grammar::AccessLogParserBaseListener,
	       public antlr4::BaseErrorListener {

public:
  bool parse_success = true;
  absl::optional<std::string> error_message = absl::nullopt;
  
  std::function<void(const std::string)> simple_expr_cb;

  std::function<void(const std::string)> plain_text_cb;

  std::function<void(const std::string,
		     const std::string,
		     absl::optional<std::string>,
		     absl::optional<std::string>)> replace_expr_cb;


  void enterStart(access_log_grammar::AccessLogParser::StartContext* ctx);

  void enterSimple_expr(access_log_grammar::AccessLogParser::Simple_exprContext* ctx);
  
  void enterLookup_expr(access_log_grammar::AccessLogParser::Lookup_exprContext* ctx);

  void syntaxError(antlr4::Recognizer* r, antlr4::Token* t, size_t, size_t,
		   const std::string& m, std::exception_ptr) override;
};
    
}
}
