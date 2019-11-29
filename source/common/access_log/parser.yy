%skeleton "lalr1.cc" /* -*- C++ -*- */
%require "3.4"
%defines

%define api.token.constructor
%define api.value.type variant
%define parse.assert

%code requires {
#include <string>
#include "absl/types/optional.h"

namespace Envoy {
namespace AccessLog {
class Driver;
}
}
 
}

// The parsing context.
%param { Envoy::AccessLog::Driver &drv }

%locations

%define parse.trace
%define parse.error verbose

%code {
#include "common/access_log/driver.h"
}

%define api.token.prefix {TOK_}
%token
  END  0  "end of file"
;

%token <std::string> CMD "%"
%token <std::string> LBR "("
%token <std::string> RBR ")"
%token <std::string> COL ":"
%token <std::string> ALT        "?";
%token <std::string> TEXT       "text"
%token <std::string> SELECTOR   "textcol"
%token <std::string> SIZE       "size"

%printer { yyo << $$; } <*>;

%%
%start unit;

unit: log_format  { };

log_format:
%empty                 {}
|	log_format lookup_expr  { }
|       log_format simple_expr  { }
|       log_format "text" { drv.plain_text_cb($2); };

simple_expr:
"%" "text" "%" { drv.simple_expr_cb($2); };

lookup_expr:
 "%" "text" "(" "textcol" "?" "textcol" ")" ":" "size" "%" {  drv.replace_expr_cb($2, $4, $6, $9); };
| "%" "text" "(" "textcol" ")" ":" "size" "%" { drv.replace_expr_cb($2, $4, absl::nullopt, $7); };
| "%" "text" "(" "textcol" "?" "textcol" ")" "%" {  drv.replace_expr_cb($2, $4, $6, absl::nullopt); };
| "%" "text" "(" "textcol" ")" "%" {  drv.replace_expr_cb($2, $4, absl::nullopt, absl::nullopt) ;};

%%

void
yy::parser::error (const location_type& l, const std::string& m)
{
  std::cerr << l << ": " << m << '\n';
}
