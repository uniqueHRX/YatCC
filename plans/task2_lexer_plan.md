# 实验二词法分析器补全计划

## 任务概述

补全 [`SYsULexer.tokens`](task/2/antlr/SYsULexer.tokens) 和 [`SYsULexer.cpp`](task/2/antlr/SYsULexer.cpp) 中的 `kClangTokens` 映射，使其包含测试样例中涉及的所有 token 名字。

## 分析结果

### 1. 现有 SYsULexer.tokens 内容（15个token）

| Token名 | 编号 | Clang名称 | 说明 |
|---------|------|-----------|------|
| Int | 1 | `int` | int关键字 |
| Identifier | 2 | `identifier` | 标识符 |
| LeftParen | 3 | `l_paren` | 左圆括号 `(` |
| RightParen | 4 | `r_paren` | 右圆括号 `)` |
| Return | 5 | `return` | return关键字 |
| RightBrace | 6 | `r_brace` | 右花括号 `}` |
| LeftBrace | 7 | `l_brace` | 左花括号 `{` |
| Constant | 8 | `numeric_constant` | 数字常量 |
| Semi | 9 | `semi` | 分号 `;` |
| Equal | 10 | `equal` | 等号 `=` |
| Plus | 11 | `plus` | 加号 `+` |
| Minus | 12 | `minus` | 减号 `-` |
| Comma | 13 | `comma` | 逗号 `,` |
| LeftBracket | 14 | `l_square` | 左方括号 `[` |
| RightBracket | 15 | `r_square` | 右方括号 `]` |

### 2. 需要新增的 Token（基于测试用例分析）

#### 关键字类 Token

| Token名 | 编号 | Clang名称 | 说明 |
|---------|------|-----------|------|
| Void | 16 | `void` | void类型关键字 |
| Const | 17 | `const` | const修饰符关键字 |
| If | 18 | `if` | if语句关键字 |
| Else | 19 | `else` | else语句关键字 |
| While | 20 | `while` | while循环关键字 |
| Break | 21 | `break` | break语句关键字 |
| Continue | 22 | `continue` | continue语句关键字 |

#### 运算符类 Token

| Token名 | 编号 | Clang名称 | 说明 |
|---------|------|-----------|------|
| Star | 23 | `star` | 星号 `*`（乘法/指针） |
| Slash | 24 | `slash` | 斜杠 `/`（除法） |
| Percent | 25 | `percent` | 百分号 `%`（取模） |
| Less | 26 | `less` | 小于号 `<` |
| Greater | 27 | `greater` | 大于号 `>` |
| EqualEqual | 28 | `equalequal` | 双等号 `==` |
| Exclaim | 29 | `exclaim` | 感叹号 `!`（逻辑非） |
| ExclaimEqual | 30 | `exclaimequal` | 不等号 `!=` |
| LessEqual | 31 | `lessequal` | 小于等于 `<=` |
| GreaterEqual | 32 | `greaterequal` | 大于等于 `>=` |
| AmpAmp | 33 | `ampamp` | 双与号 `&&`（逻辑与） |
| PipePipe | 34 | `pipepipe` | 双或号 `||`（逻辑或） |

## 实施步骤

### 步骤1：补全 SYsULexer.tokens 文件

在现有内容基础上，添加上述22个新token定义，编号从16开始连续递增。

**修改后的完整内容：**
```
Int=1
Identifier=2
LeftParen=3
RightParen=4
Return=5
RightBrace=6
LeftBrace=7
Constant=8
Semi=9
Equal=10
Plus=11
Minus=12
Comma=13
LeftBracket=14
RightBracket=15
Void=16
Const=17
If=18
Else=19
While=20
Break=21
Continue=22
Star=23
Slash=24
Percent=25
Less=26
Greater=27
EqualEqual=28
Exclaim=29
ExclaimEqual=30
LessEqual=31
GreaterEqual=32
AmpAmp=33
PipePipe=34
```

### 步骤2：补全 SYsULexer.cpp 中的 kClangTokens 映射

在现有 `kClangTokens` 映射表中添加新的映射条目。

**需要添加的映射：**
```cpp
{ "void", kVoid },
{ "const", kConst },
{ "if", kIf },
{ "else", kElse },
{ "while", kWhile },
{ "break", kBreak },
{ "continue", kContinue },
{ "star", kStar },
{ "slash", kSlash },
{ "percent", kPercent },
{ "less", kLess },
{ "greater", kGreater },
{ "equalequal", kEqualEqual },
{ "exclaim", kExclaim },
{ "exclaimequal", kExclaimEqual },
{ "lessequal", kLessEqual },
{ "greaterequal", kGreaterEqual },
{ "ampamp", kAmpAmp },
{ "pipepipe", kPipePipe }
```

## Clang Token 命名规范说明

Clang 的 token 命名遵循以下规则：

1. **关键字**：直接使用关键字本身的小写形式，如 `int`、`return`、`if`、`while`
2. **标识符**：使用 `identifier`
3. **常量**：使用 `numeric_constant`（数字）、`string_literal`（字符串）等
4. **括号类**：
   - `l_paren` / `r_paren` - 圆括号 `(` `)`
   - `l_brace` / `r_brace` - 花括号 `{` `}`
   - `l_square` / `r_square` - 方括号 `[` `]`
5. **运算符**：
   - 单字符：直接用名称，如 `plus`、`minus`、`star`、`slash`、`percent`、`less`、`greater`、`equal`、`exclaim`
   - 双字符：组合命名，如 `equalequal`、`exclaimequal`、`lessequal`、`greaterequal`、`ampamp`、`pipepipe`
6. **分隔符**：`semi`（分号）、`comma`（逗号）
7. **结束符**：`eof`

## 验证方法

完成修改后，可通过以下方式验证：

1. 构建项目，检查 [`SYsULexer.tokens.hpp`](task/2/antlr/SYsULexer.tokens.hpp) 是否正确生成
2. 运行测试用例，确认词法分析器能正确识别所有新增的 token 类型

## 注意事项

1. Token 编号必须连续且唯一
2. Clang token 名称必须与预处理输出格式完全匹配（小写、无空格）
3. `SYsULexer.py` 会根据 `.tokens` 文件自动生成 `.tokens.hpp` 头文件
4. 新增的 token 主要用于支持更完整的 SysY 语言语法（if/else/while/const 等）