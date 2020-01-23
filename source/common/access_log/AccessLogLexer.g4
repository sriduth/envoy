lexer grammar AccessLogLexer;

CATCH_ALL : ~[%]+;
DELIM : '%' -> pushMode(found_delimiter);

mode found_delimiter;
COL: ':';
ALT: '?';
LBR: '(' -> pushMode(inside_lookup);
RBR: ')';
INT : [0-9]+;
TEXT : [a-zA-Z0-9_]+;
EXIT: '%' -> type(DELIM), popMode;


mode inside_lookup;
KEY : ~[\\)]+ -> popMode;