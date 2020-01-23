#include "absl/types/optional.h"
#include "common/access_log/driver.h"

namespace Envoy {
namespace AccessLog {

  void Driver::enterStart(access_log_grammar::AccessLogParser::StartContext *ctx) {
    antlr4::tree::TerminalNode* catch_all =
      ctx->CATCH_ALL();

    if (catch_all != nullptr) {
      plain_text_cb(catch_all->getText());
    }
  }

  void Driver::enterSimple_expr(access_log_grammar::AccessLogParser::Simple_exprContext* ctx) {
    antlr4::tree::TerminalNode *lookup_key = ctx->TEXT();
    
    if(lookup_key == nullptr) {
      parse_success = false;
      return;
    }
    
    simple_expr_cb(lookup_key->getText());
  }
  
  void Driver::enterLookup_expr(access_log_grammar::AccessLogParser::Lookup_exprContext* ctx) {
    antlr4::tree::TerminalNode* function = ctx->TEXT();
    antlr4::tree::TerminalNode* key = ctx->KEY();
    antlr4::tree::TerminalNode* length = ctx->INT();

    absl::optional<std::string> alt_ = absl::nullopt;
    absl::optional<std::string> length_ = absl::nullopt;
    
    if(length != nullptr) {
      length_ = length->getText();
    }

    if(function == nullptr || key == nullptr) {
      parse_success = false;
      return;
    }

    std::string key_text = key->getText();

    if(key_text.find('\n') != std::string::npos) {
      parse_success = false;
      return;
    }
    
    if(key_text.find('?') != std::string::npos) {
      const std::vector<std::string> tokens = absl::StrSplit(key_text, '?');

      if(tokens.size() > 2) {
	parse_success = false;
	return;
      }
      
      key_text = tokens[0];
      alt_ = tokens[1];
    }

    replace_expr_cb(function->getText(), key_text, alt_, length_);
  }

  void Driver::syntaxError(antlr4::Recognizer*, antlr4::Token*, size_t, size_t,
		   const std::string& m, std::exception_ptr) {
    
    parse_success = false;
    error_message = m;
  }

}
}
