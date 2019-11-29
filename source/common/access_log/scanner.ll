%{ /* -*- C++ -*- */
#pragma GCC diagnostic ignored "-Wold-style-cast"
# include <cerrno>
# include <climits>
# include <cstdlib>
# include <cstring> // strerror
# include <string>
# include "common/access_log/driver.h"
# include "common/access_log/parser.hh"
%}

%x SEL

%option noyywrap nounput noinput batch debug

cmd     [%]
col     [:]
alt     [\?]
b1      [\(]
b2      [\)]
id      [a-zA-Z\.][a-zA-Z_0-9\-\.]*
sel     [a-zA-Z_0-9\-\.:\%[:blank:]\/\|]+
number  [0-9]+
space   [[:space:]]+

%{
  // Code run each time a pattern is matched.
  # define YY_USER_ACTION  loc.columns (yyleng);
%}
%%
%{
  // A handy shortcut to the location held by the driver.
  yy::location& loc = drv.location;
  // Code run each time yylex is called.
  loc.step ();
%}
<*>{space}    { loc.lines(yyleng); loc.step(); drv.plain_text_cb(yytext); }
<*>{number}   return yy::parser::make_SIZE(yytext, loc);
{col}       return yy::parser::make_COL(yytext, loc);
<INITIAL>{cmd}      return yy::parser::make_CMD(yytext, loc);
<*>{b1}       { BEGIN(SEL); return yy::parser::make_LBR(yytext, loc); }
<*>{b2}       { BEGIN(INITIAL); return yy::parser::make_RBR(yytext, loc); }
<*>{alt}      return yy::parser::make_ALT (yytext, loc);
<SEL>{sel}    return yy::parser::make_SELECTOR (yytext, loc);
<*>{id}       return yy::parser::make_TEXT (yytext, loc);
.          return yy::parser::make_TEXT (yytext, loc);
<<EOF>>    return yy::parser::make_END (loc);
%%

void Envoy::AccessLog::Driver::scan_begin (std::string log_format)
{
  yy_flex_debug = trace_scanning;
  yy_scan_string(log_format.c_str());
}

void Envoy::AccessLog::Driver::scan_end ()
{
  yylex_destroy();
}
