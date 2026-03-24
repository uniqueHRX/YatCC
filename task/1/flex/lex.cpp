#include "lex.hpp"
#include <iostream>

void
print_token();

namespace lex {

static const char* kTokenNames[] = {
  "identifier",        // IDENTIFIER
  "numeric_constant",  // NUMERIC_CONSTANT
  "string_literal",    // STRING_LITERAL
  "char_constant",     // CHAR_CONSTANT
  "char",              // CHAR
  "short",             // SHORT
  "int",               // INT
  "long",              // LONG
  "float",             // FLOAT
  "double",            // DOUBLE
  "void",              // VOID
  "signed",            // SIGNED
  "unsigned",          // UNSIGNED
  "struct",            // STRUCT
  "union",             // UNION
  "enum",              // ENUM
  "typedef",           // TYPEDEF
  "extern",            // EXTERN
  "static",            // STATIC
  "auto",              // AUTO
  "register",          // REGISTER
  "const",             // CONST
  "volatile",          // VOLATILE
  "restrict",          // RESTRICT
  "inline",            // INLINE
  "if",                // IF
  "else",              // ELSE
  "while",             // WHILE
  "do",                // DO
  "for",               // FOR
  "switch",            // SWITCH
  "case",              // CASE
  "default",           // DEFAULT
  "break",             // BREAK
  "continue",          // CONTINUE
  "return",            // RETURN
  "goto",              // GOTO
  "sizeof",            // SIZEOF
  "l_paren",           // L_PAREN
  "r_paren",           // R_PAREN
  "l_square",          // L_SQUARE
  "r_square",          // R_SQUARE
  "l_brace",           // L_BRACE
  "r_brace",           // R_BRACE
  "plus",              // PLUS
  "minus",             // MINUS
  "star",              // STAR
  "slash",             // SLASH
  "percent",           // PERCENT
  "amp",               // AMP
  "pipe",              // PIPE
  "caret",             // CARET
  "tilde",             // TILDE
  "exclaim",           // EXCLAIM
  "less",              // LESS
  "greater",           // GREATER
  "equal",             // EQUAL
  "question",          // QUESTION
  "colon",             // COLON
  "semi",              // SEMI
  "comma",             // COMMA
  "period",            // PERIOD
  "arrow",             // ARROW
  "plusplus",          // PLUSPLUS
  "minusminus",        // MINUSMINUS
  "lessless",          // LESSLESS
  "greatergreater",    // GREATERGREATER
  "lesslessequal",     // LESSLESSEQUAL
  "greatergreaterequal", // GREATERGREATEREQUAL
  "ampamp",            // AMPAMP
  "pipepipe",          // PIPEPIPE
  "equalequal",        // EQUALEQUAL
  "exclaimequal",      // EXCLAIMEQUAL
  "lessequal",         // LESSEQUAL
  "greaterequal",      // GREATEREQUAL
  "plusequal",         // PLUSEQUAL
  "minusequal",        // MINUSEQUAL
  "starequal",         // STAREQUAL
  "slashequal",        // SLASHEQUAL
  "percentequal",      // PERCENTEQUAL
  "ampequal",          // AMPEQUAL
  "pipeequal",         // PIPEEQUAL
  "caretequal",        // CARETEQUAL
  "ellipsis"           // ELLIPSIS
};

const char*
id2str(Id id)
{
  static char sCharBuf[2] = { 0, 0 };
  if (id == Id::YYEOF) {
    return "eof";
  }
  else if (id < Id::IDENTIFIER) {
    sCharBuf[0] = char(id);
    return sCharBuf;
  }
  return kTokenNames[int(id) - int(Id::IDENTIFIER)];
}

G g;

int
come(int tokenId, const char* yytext, int yyleng, int yylineno)
{
  g.mId = Id(tokenId);
  g.mText = { yytext, std::size_t(yyleng) };
  g.mLine = yylineno;

  print_token();
  g.mStartOfLine = false;
  g.mLeadingSpace = false;

  return tokenId;
}

void
space(char c)
{
  if (c == '\n') {
    g.mLine++;
    g.mColumn = 1;
    g.mStartOfLine = true;
    g.mLeadingSpace = false;
  } else if (c == ' ' || c == '\t') {
    g.mColumn++;
    if (!g.mStartOfLine) {
      g.mLeadingSpace = true;
    }
  }
  // other whitespace characters like \v, \f can be handled similarly
}

void
extract_preprocessed_info(const char* line)
{
  // line format: # 123 "filename"
  // we need to extract line number and filename
  // skip '#'
  const char* p = line + 1;
  while (*p && (*p == ' ' || *p == '\t')) p++;
  // read line number
  int lineNum = 0;
  while (*p >= '0' && *p <= '9') {
    lineNum = lineNum * 10 + (*p - '0');
    p++;
  }
  // skip spaces
  while (*p && (*p == ' ' || *p == '\t')) p++;
  // expect double quote
  if (*p != '"') return;
  p++;
  const char* filenameStart = p;
  while (*p && *p != '"') p++;
  if (*p != '"') return;
  std::string filename(filenameStart, p - filenameStart);
  // update global state
  g.mFile = filename;
  // line number should be decremented by 1 because the newline after this line will increment it
  g.mLine = lineNum - 1;
  g.mColumn = 1;
  g.mStartOfLine = true;
  g.mLeadingSpace = false;
}

} // namespace lex
