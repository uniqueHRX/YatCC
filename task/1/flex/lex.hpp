#pragma once

#include <string>
#include <string_view>
#include <cstring>

namespace lex {

enum Id
{
  YYEMPTY = -2,
  YYEOF = 0,     /* "end of file"  */
  YYerror = 256, /* error  */
  YYUNDEF = 257, /* "invalid token"  */
  IDENTIFIER,
  NUMERIC_CONSTANT,
  STRING_LITERAL,
  CHAR_CONSTANT,
  // Keywords (35)
  CHAR,
  SHORT,
  INT,
  LONG,
  FLOAT,
  DOUBLE,
  VOID,
  SIGNED,
  UNSIGNED,
  STRUCT,
  UNION,
  ENUM,
  TYPEDEF,
  EXTERN,
  STATIC,
  AUTO,
  REGISTER,
  CONST,
  VOLATILE,
  RESTRICT,
  INLINE,
  IF,
  ELSE,
  WHILE,
  DO,
  FOR,
  SWITCH,
  CASE,
  DEFAULT,
  BREAK,
  CONTINUE,
  RETURN,
  GOTO,
  SIZEOF,
  // Punctuators and operators
  L_PAREN,
  R_PAREN,
  L_SQUARE,
  R_SQUARE,
  L_BRACE,
  R_BRACE,
  PLUS,
  MINUS,
  STAR,
  SLASH,
  PERCENT,
  AMP,
  PIPE,
  CARET,
  TILDE,
  EXCLAIM,
  LESS,
  GREATER,
  EQUAL,
  QUESTION,
  COLON,
  SEMI,
  COMMA,
  PERIOD,
  ARROW,
  PLUSPLUS,
  MINUSMINUS,
  LESSLESS,
  GREATERGREATER,
  LESSLESSEQUAL,
  GREATERGREATEREQUAL,
  AMPAMP,
  PIPEPIPE,
  EQUALEQUAL,
  EXCLAIMEQUAL,
  LESSEQUAL,
  GREATEREQUAL,
  PLUSEQUAL,
  MINUSEQUAL,
  STAREQUAL,
  SLASHEQUAL,
  PERCENTEQUAL,
  AMPEQUAL,
  PIPEEQUAL,
  CARETEQUAL,
  ELLIPSIS
};

const char*
id2str(Id id);

struct G
{
  Id mId{ YYEOF };              // 词号
  std::string_view mText;       // 对应文本
  std::string mFile;            // 文件路径
  int mLine{ 1 }, mColumn{ 1 }; // 行号、列号
  bool mStartOfLine{ true };    // 是否是行首
  bool mLeadingSpace{ false };  // 是否有前导空格
};

extern G g;

int
come(int tokenId, const char* yytext, int yyleng, int yylineno);

void
space(char c);

void
extract_preprocessed_info(const char* line);

} // namespace lex
