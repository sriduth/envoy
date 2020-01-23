parser grammar AccessLogParser;

options { tokenVocab=AccessLogLexer; }

@parser::baselistenerpreinclude {#pragma GCC diagnostic ignored "-Woverloaded-virtual"}

start
    : CATCH_ALL start
    | log_format start
    | EOF
    ;

log_format
    : lookup_expr
    | simple_expr
    ;

lookup_expr
    : DELIM TEXT LBR KEY RBR COL INT DELIM
    | DELIM TEXT LBR KEY RBR DELIM
    ;

simple_expr
    : DELIM TEXT DELIM
    ;
