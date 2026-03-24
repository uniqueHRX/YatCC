---BEGIN PROMPT---
# SYsU 语言词法分析器实现任务

## 1. 任务概述

你是一位编译原理课程助教，需要实现 **SYsU 语言**（中山大学自定义的类 C 语言子集）的**词法分析器**（Lexer/Tokenizer）。

**输入**：经过 `clang -E` 预处理后的源代码，包含 `# linenum "filename"` 预处理行指令。

**输出**：与 `clang -cc1 -dump-tokens` 格式完全兼容的 token 序列。

## 2. 输出格式精确规格

每行输出一个 token，格式如下：

```
<token_type> '<token_text>'<\t>[StartOfLine]<\t>[LeadingSpace]<\t>Loc=<<file>:<line>:<col>>
```

### 格式细节规则（必须严格遵守）

1. **token_type**：小写的 token 类型名（如 `int`, `identifier`, `l_paren`, `numeric_constant`）
2. **token_text**：token 的原始文本，用单引号包裹
3. **[StartOfLine]**：仅当该 token 是逻辑行的第一个可见 token 时输出，前面加 `\t`
4. **[LeadingSpace]**：仅当该 token 前有空白字符（且不是行首 token）时输出，前面加 `\t`
5. 若 `[StartOfLine]` 和 `[LeadingSpace]` 都不存在，输出一个 `\t` 占位
6. **Loc=<file:line:col>**：逻辑位置信息，file 是预处理行指令中的文件名，line/col 是逻辑行号和列号（1-based）
7. 最后一个 token 必须是 `eof ''`，text 为空字符串

### 示例输入
```c
# 1 "./basic/000_main.sysu.c"
int main(){
    return 3;
}
```

### 示例输出
```
int 'int'    [StartOfLine]  Loc=<./basic/000_main.sysu.c:1:1>
identifier 'main'    [LeadingSpace] Loc=<./basic/000_main.sysu.c:1:5>
l_paren '('     Loc=<./basic/000_main.sysu.c:1:9>
r_paren ')'     Loc=<./basic/000_main.sysu.c:1:10>
l_brace '{'     Loc=<./basic/000_main.sysu.c:1:11>
return 'return'  [StartOfLine] [LeadingSpace]   Loc=<./basic/000_main.sysu.c:2:5>
numeric_constant '3'     [LeadingSpace] Loc=<./basic/000_main.sysu.c:2:12>
semi ';'        Loc=<./basic/000_main.sysu.c:2:13>
r_brace '}'  [StartOfLine]  Loc=<./basic/000_main.sysu.c:3:1>
eof ''      Loc=<./basic/000_main.sysu.c:3:2>
```

## 3. 完整 Token 类型清单

### 3.1 关键字 → token_type（共 35 个）
| 关键字 | token_type |
|--------|-----------|
| `char` | `char` |
| `short` | `short` |
| `int` | `int` |
| `long` | `long` |
| `float` | `float` |
| `double` | `double` |
| `void` | `void` |
| `signed` | `signed` |
| `unsigned` | `unsigned` |
| `struct` | `struct` |
| `union` | `union` |
| `enum` | `enum` |
| `typedef` | `typedef` |
| `extern` | `extern` |
| `static` | `static` |
| `auto` | `auto` |
| `register` | `register` |
| `const` | `const` |
| `volatile` | `volatile` |
| `restrict` | `restrict` |
| `inline` | `inline` |
| `if` | `if` |
| `else` | `else` |
| `while` | `while` |
| `do` | `do` |
| `for` | `for` |
| `switch` | `switch` |
| `case` | `case` |
| `default` | `default` |
| `break` | `break` |
| `continue` | `continue` |
| `return` | `return` |
| `goto` | `goto` |
| `sizeof` | `sizeof` |

### 3.2 标点符号和运算符
| 符号 | token_type |
|------|-----------|
| `(` | `l_paren` |
| `)` | `r_paren` |
| `[` | `l_square` |
| `]` | `r_square` |
| `{` | `l_brace` |
| `}` | `r_brace` |
| `+` | `plus` |
| `-` | `minus` |
| `*` | `star` |
| `/` | `slash` |
| `%` | `percent` |
| `&` | `amp` |
| `\|` | `pipe` |
| `^` | `caret` |
| `~` | `tilde` |
| `!` | `exclaim` |
| `<` | `less` |
| `>` | `greater` |
| `=` | `equal` |
| `?` | `question` |
| `:` | `colon` |
| `;` | `semi` |
| `,` | `comma` |
| `.` | `period` |
| `->` | `arrow` |
| `++` | `plusplus` |
| `--` | `minusminus` |
| `<<` | `lessless` |
| `>>` | `greatergreater` |
| `<<=` | `lesslessequal` |
| `>>=` | `greatergreaterequal` |
| `&&` | `ampamp` |
| `\|\|` | `pipepipe` |
| `==` | `equalequal` |
| `!=` | `exclaimequal` |
| `<=` | `lessequal` |
| `>=` | `greaterequal` |
| `+=` | `plusequal` |
| `-=` | `minusequal` |
| `*=` | `starequal` |
| `/=` | `slashequal` |
| `%=` | `percentequal` |
| `&=` | `ampequal` |
| `\|=` | `pipeequal` |
| `^=` | `caretequal` |
| `...` | `ellipsis` |

### 3.3 其他 token 类型
| 类别 | token_type | 说明 |
|------|-----------|------|
| 标识符 | `identifier` | `[a-zA-Z_][a-zA-Z0-9_]*` |
| 数值常量 | `numeric_constant` | 十进制/八进制/十六进制整数及浮点数（含后缀） |
| 字符串字面量 | `string_literal` | `"..."` 含转义序列，可选 `L` 前缀 |
| 字符常量 | `char_constant` | `'...'` 含转义序列，可选 `L` 前缀 |
| 文件结束 | `eof` | text 为空字符串 |

## 4. 关键处理逻辑

### 4.1 预处理行解析
- 形如 `# 1 "./basic/000_main.sysu.c"` 的行**不输出**为 token
- 从中提取**逻辑文件名**（引号之间的部分）和**逻辑行号**（数字部分）
- 提取的行号赋值后需 **减 1**（因为紧跟的换行会将行号 +1）
- 列号重置为 1

### 4.2 空白字符跟踪
- 空格和制表符 `[ \t]`：不输出为 token，但需更新 `logicalColumn += 字符长度`，并标记 `leadingSpace = true`
- 换行 `\n`：不输出为 token，但需更新 `logicalLine++`，`logicalColumn = 1`，标记 `startOfLine = true`，`leadingSpace = false`

### 4.3 注释处理
- 行注释 `// ...`：跳过，不输出
- 块注释 `/* ... */`：跳过，不输出

### 4.4 多字符运算符优先级（最长匹配）
- `<<=` 优先于 `<<` 优先于 `<`
- `>>=` 优先于 `>>` 优先于 `>`
- `...` 优先于 `.`
- `->` 优先于 `-`
- `++` 优先于 `+`
- `--` 优先于 `-`
- 关键字识别优先于标识符（如 `int` 是关键字，不是标识符）

### 4.5 数值常量正则
```
十进制整数：    [1-9][0-9]*  (后缀可选: u/U, l/L/ll/LL 及其组合)
八进制整数：    0[0-7]*      (后缀可选)
十六进制整数：  0[xX][0-9a-fA-F]+   (后缀可选)
十进制浮点：    [0-9]*\.[0-9]+ ([eE][+-]?[0-9]+)? [fFlL]?
               [0-9]+\.[0-9]* ([eE][+-]?[0-9]+)? [fFlL]?
               [0-9]+ [eE][+-]?[0-9]+ [fFlL]?
十六进制浮点：  0[xX][0-9a-fA-F]*\.[0-9a-fA-F]+ [pP][+-]?[0-9]+ [fFlL]?
               0[xX][0-9a-fA-F]+\.[0-9a-fA-F]* [pP][+-]?[0-9]+ [fFlL]?
               0[xX][0-9a-fA-F]+ [pP][+-]?[0-9]+ [fFlL]?
```

## 5. 评分标准
- **60%** — token 类型名 + 文本值完全正确
- **30%** — `Loc=<file:line:col>` 位置信息完全正确
- **10%** — `[StartOfLine]` / `[LeadingSpace]` 标志完全正确
- token 总数不匹配直接判零分

## 6. 实现路径选择：Flex 实现（C++17）

### 需要生成的文件：
1. **`lex.l`**：Flex 词法规则文件
2. **`lex.hpp`**：token 枚举 `Id`、全局状态 `G g`、辅助函数声明
3. **`lex.cpp`**：token 名映射表 `kTokenNames[]`、`come()`/`extract_preprocessed_info()`/`space()` 实现
4. **`main.cpp`**：驱动程序（打开文件、`while(yylex())` 循环、`print_token()` 输出）

### 技术要点：
1. **`lex.l` 文件结构**：
   - 使用 `%{ ... %}` 包含头文件和宏定义
   - 定义正则表达式别名（如 `D [0-9]`, `L [a-zA-Z_]`）
   - 按照最长匹配原则排列规则（多字符运算符在前）
   - 使用宏 `ADDCOL()` 更新列号，`COME(id)` 返回 token
   - 预处理行使用特殊规则解析并更新逻辑位置
   - 注释和空白使用特殊规则处理，不返回 token

2. **`lex.hpp` 实现要点**：
   - 定义 `enum Id` 包含所有 token 类型（包括 `YYEOF`、`YYerror` 等）
   - 定义 `struct G` 包含：`mId`, `mText`, `mFile`, `mLine`, `mColumn`, `mStartOfLine`, `mLeadingSpace`
   - 声明全局变量 `extern G g`
   - 声明辅助函数：`come()`, `space()`, `extract_preprocessed_info()`

3. **`lex.cpp` 实现要点**：
   - 定义 `kTokenNames[]` 数组，将 `Id` 映射到 clang 格式的 token 类型名
   - 实现 `come()` 函数：更新全局状态 `g`，返回 token id
   - 实现 `space()` 函数：处理空白字符，更新行列号
   - 实现 `extract_preprocessed_info()`：解析 `# linenum "filename"` 格式，更新文件名和行号（行号减1）

4. **`main.cpp` 实现要点（C++17）**：
   - 使用 C++17 标准编译
   - 打开输入/输出文件，设置 `yyin` 输入流
   - `while(yylex())` 循环获取 token，调用 `print_token()` 输出
   - 处理 `<<EOF>>` 特殊规则，返回 `YYEOF`
   - 输出格式必须严格遵循第2节定义的格式

### 关键代码模式：
```flex
%{
#include "lex.hpp"
#define ADDCOL() g.mColumn += yyleng;
#define COME(id) return come(id, yytext, yyleng, yylineno)
%}

%option 8bit warn noyywrap yylineno

D     [0-9]
L     [a-zA-Z_]

%%

"int"       { ADDCOL(); COME(INT); }
"return"    { ADDCOL(); COME(RETURN); }
"("         { ADDCOL(); COME(L_PAREN); }

{L}({L}|{D})* { ADDCOL(); COME(IDENTIFIER); }

^#[^\n]*    { /* 预处理行处理 */ }

[ \t]       { space(); return ~YYEOF; }
\n          { space(); return ~YYEOF; }

<<EOF>>     { ADDCOL(); COME(YYEOF); }
```

### 边界情况提示：
1. **预处理行解析**：行号需要减1，因为换行符会使行号+1
2. **转义序列**：字符串和字符常量中的 `\n`, `\t`, `\\`, `\"`, `\'` 等
3. **十六进制浮点数**：包含 `p` 或 `P` 指数（不是 `e`）
4. **后缀组合**：整数后缀如 `uLL`, `LU` 等需要完整匹配
5. **注释嵌套**：块注释不支持嵌套
6. **关键字优先**：关键字规则必须在标识符规则之前
7. **EOF 处理**：必须输出 `eof ''` 作为最后一个 token
8. **空白处理**：`space()` 函数需要正确更新 `mStartOfLine` 和 `mLeadingSpace` 标志


---END PROMPT---