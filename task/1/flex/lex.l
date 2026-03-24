%{
#include "lex.hpp"
/* 所有代码全部抽离出来，放到 lex.hpp 和 lex.cpp 里 */

using namespace lex;

#define ADDCOL() g.mColumn += yyleng;
#define COME(id) return come(id, yytext, yyleng, yylineno)
%}

%option 8bit warn noyywrap yylineno

D     [0-9]
L     [a-zA-Z_]
H     [0-9a-fA-F]
IS    ((u|U)|(u|U)?(l|L|ll|LL)|(l|L|ll|LL)(u|U))
FS    [fFlL]?

%%

^#[^\n]*              { extract_preprocessed_info(yytext); }

[ \t\v\f]             { for (int i = 0; i < yyleng; i++) space(yytext[i]); }
\n                    { for (int i = 0; i < yyleng; i++) space(yytext[i]); }

"/*"([^*]|\*+[^*/])*"*/"   { /* block comment, ignore */ }
"//"[^\n]*               { /* line comment, ignore */ }

"..."                    { ADDCOL(); COME(ELLIPSIS); }
"<<="                    { ADDCOL(); COME(LESSLESSEQUAL); }
">>="                    { ADDCOL(); COME(GREATERGREATEREQUAL); }
"->"                     { ADDCOL(); COME(ARROW); }
"++"                     { ADDCOL(); COME(PLUSPLUS); }
"--"                     { ADDCOL(); COME(MINUSMINUS); }
"<<"                     { ADDCOL(); COME(LESSLESS); }
">>"                     { ADDCOL(); COME(GREATERGREATER); }
"<="                     { ADDCOL(); COME(LESSEQUAL); }
">="                     { ADDCOL(); COME(GREATEREQUAL); }
"=="                     { ADDCOL(); COME(EQUALEQUAL); }
"!="                     { ADDCOL(); COME(EXCLAIMEQUAL); }
"&&"                     { ADDCOL(); COME(AMPAMP); }
"||"                     { ADDCOL(); COME(PIPEPIPE); }
"+="                     { ADDCOL(); COME(PLUSEQUAL); }
"-="                     { ADDCOL(); COME(MINUSEQUAL); }
"*="                     { ADDCOL(); COME(STAREQUAL); }
"/="                     { ADDCOL(); COME(SLASHEQUAL); }
"%="                     { ADDCOL(); COME(PERCENTEQUAL); }
"&="                     { ADDCOL(); COME(AMPEQUAL); }
"|="                     { ADDCOL(); COME(PIPEEQUAL); }
"^="                     { ADDCOL(); COME(CARETEQUAL); }

"("                      { ADDCOL(); COME(L_PAREN); }
")"                      { ADDCOL(); COME(R_PAREN); }
"["                      { ADDCOL(); COME(L_SQUARE); }
"]"                      { ADDCOL(); COME(R_SQUARE); }
"{"                      { ADDCOL(); COME(L_BRACE); }
"}"                      { ADDCOL(); COME(R_BRACE); }
"+"                      { ADDCOL(); COME(PLUS); }
"-"                      { ADDCOL(); COME(MINUS); }
"*"                      { ADDCOL(); COME(STAR); }
"/"                      { ADDCOL(); COME(SLASH); }
"%"                      { ADDCOL(); COME(PERCENT); }
"&"                      { ADDCOL(); COME(AMP); }
"|"                      { ADDCOL(); COME(PIPE); }
"^"                      { ADDCOL(); COME(CARET); }
"~"                      { ADDCOL(); COME(TILDE); }
"!"                      { ADDCOL(); COME(EXCLAIM); }
"<"                      { ADDCOL(); COME(LESS); }
">"                      { ADDCOL(); COME(GREATER); }
"="                      { ADDCOL(); COME(EQUAL); }
"?"                      { ADDCOL(); COME(QUESTION); }
":"                      { ADDCOL(); COME(COLON); }
";"                      { ADDCOL(); COME(SEMI); }
","                      { ADDCOL(); COME(COMMA); }
"."                      { ADDCOL(); COME(PERIOD); }

"char"                   { ADDCOL(); COME(CHAR); }
"short"                  { ADDCOL(); COME(SHORT); }
"int"                    { ADDCOL(); COME(INT); }
"long"                   { ADDCOL(); COME(LONG); }
"float"                  { ADDCOL(); COME(FLOAT); }
"double"                 { ADDCOL(); COME(DOUBLE); }
"void"                   { ADDCOL(); COME(VOID); }
"signed"                 { ADDCOL(); COME(SIGNED); }
"unsigned"               { ADDCOL(); COME(UNSIGNED); }
"struct"                 { ADDCOL(); COME(STRUCT); }
"union"                  { ADDCOL(); COME(UNION); }
"enum"                   { ADDCOL(); COME(ENUM); }
"typedef"                { ADDCOL(); COME(TYPEDEF); }
"extern"                 { ADDCOL(); COME(EXTERN); }
"static"                 { ADDCOL(); COME(STATIC); }
"auto"                   { ADDCOL(); COME(AUTO); }
"register"               { ADDCOL(); COME(REGISTER); }
"const"                  { ADDCOL(); COME(CONST); }
"volatile"               { ADDCOL(); COME(VOLATILE); }
"restrict"               { ADDCOL(); COME(RESTRICT); }
"inline"                 { ADDCOL(); COME(INLINE); }
"if"                     { ADDCOL(); COME(IF); }
"else"                   { ADDCOL(); COME(ELSE); }
"while"                  { ADDCOL(); COME(WHILE); }
"do"                     { ADDCOL(); COME(DO); }
"for"                    { ADDCOL(); COME(FOR); }
"switch"                 { ADDCOL(); COME(SWITCH); }
"case"                   { ADDCOL(); COME(CASE); }
"default"                { ADDCOL(); COME(DEFAULT); }
"break"                  { ADDCOL(); COME(BREAK); }
"continue"               { ADDCOL(); COME(CONTINUE); }
"return"                 { ADDCOL(); COME(RETURN); }
"goto"                   { ADDCOL(); COME(GOTO); }
"sizeof"                 { ADDCOL(); COME(SIZEOF); }

{L}({L}|{D})*            { ADDCOL(); COME(IDENTIFIER); }

0[0-7]*{IS}?             { ADDCOL(); COME(NUMERIC_CONSTANT); }
[1-9]{D}*{IS}?           { ADDCOL(); COME(NUMERIC_CONSTANT); }
0[xX]{H}+{IS}?           { ADDCOL(); COME(NUMERIC_CONSTANT); }

{D}+\.{D}*([eE][+-]?{D}+)?{FS}?      { ADDCOL(); COME(NUMERIC_CONSTANT); }
{D}*\.{D}+([eE][+-]?{D}+)?{FS}?      { ADDCOL(); COME(NUMERIC_CONSTANT); }
{D}+[eE][+-]?{D}+{FS}?               { ADDCOL(); COME(NUMERIC_CONSTANT); }
0[xX]{H}+\.{H}*[pP][+-]?{D}+{FS}?    { ADDCOL(); COME(NUMERIC_CONSTANT); }
0[xX]{H}*\.{H}+[pP][+-]?{D}+{FS}?    { ADDCOL(); COME(NUMERIC_CONSTANT); }
0[xX]{H}+[pP][+-]?{D}+{FS}?          { ADDCOL(); COME(NUMERIC_CONSTANT); }

L?\"(\\.|[^\\"\n])*\"    { ADDCOL(); COME(STRING_LITERAL); }

L?'(\\.|[^\\'\n])+'      { ADDCOL(); COME(CHAR_CONSTANT); }

<<EOF>>                  { ADDCOL(); COME(YYEOF); }

.                        { /* unrecognized character, ignore */ }

%%

/* about symbols avaliable (yytext, yyleng etc.) in the context of Flex:
 * https://ftp.gnu.org/old-gnu/Manuals/flex-2.5.4/html_node/flex_14.html
 * https://ftp.gnu.org/old-gnu/Manuals/flex-2.5.4/html_node/flex_15.html
 */
