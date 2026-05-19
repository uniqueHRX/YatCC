# 实验三：EmitIR 中间代码生成 — 实现计划

## 当前状态

| 指标 | 数值 |
|------|------|
| 已通过 | 1/73 (000_main.sysu.c) |
| 失败 | 72/73 (全部 Subprocess aborted) |
| 根因 | EmitIR.cpp 中大部分 ASG 节点类型未实现，触发 ABORT() |

## 核心架构理解

### 符号表机制

ASG 框架已提供天然符号表——`Obj::any` 字段：

| ASG 节点 | `obj->any` 存储内容 | 用途 |
|----------|-------------------|------|
| [`VarDecl`](task/2/common/asg.hpp:362)（全局） | [`llvm::GlobalVariable*`](task/3/EmitIR.cpp:163) | 全局变量 |
| [`VarDecl`](task/2/common/asg.hpp:362)（局部） | [`llvm::AllocaInst*`](task/3/EmitIR.cpp:未实现) | 局部变量 |
| [`FunctionDecl`](task/2/common/asg.hpp:370) | [`llvm::Function*`](task/3/EmitIR.cpp:191) | 函数定义 |
| [`DeclRefExpr`](task/2/common/asg.hpp:158) | 通过 `decl->any` 查找 | 变量引用 |

**无需额外构建符号表结构！**

### 关键设计模式

`EmitIR` 使用 function-call operator overloading 实现 visitor pattern：

```
Expr* → llvm::Value* operator()(asg::Expr* obj)     // 表达式产生值
Stmt* → void        operator()(asg::Stmt* obj)       // 语句不产生值
Decl* → void        operator()(asg::Decl* obj)        // 声明不产生值
Type* → llvm::Type* operator()(const asg::Type* type) // 类型转换
```

### ImplicitCastExpr 处理（关键！）

Typing 阶段会在 ASG 中插入各种隐式 cast，EmitIR 必须正确处理：

| Cast Kind | LLVM 指令 |
|-----------|----------|
| `kLValueToRValue` | `irb.CreateLoad(ty, subVal)` |
| `kIntegralCast` | 根据源/目标类型宽度选择 `zext`/`sext`/`trunc` |
| `kArrayToPointerDecay` | `irb.CreateGEP(arrTy, arrVal, {0, 0})` |
| `kFunctionToPointerDecay` | 直接透传 subVal |
| `kNoOp` | 直接透传 subVal |

---

## 阶段一：类型系统完善

### 目标
让 [`EmitIR::operator()(const Type*)`](task/3/EmitIR.cpp:31) 支持所有基础类型和复合类型。

### 现状
- 仅处理 `kInt` → `i32`
- `kChar`/`kVoid`/`kLong`/`kLongLong` 未处理
- 指针类型 [`PointerType`](task/2/common/asg.hpp:100) / 数组类型 [`ArrayType`](task/2/common/asg.hpp:108) / [`FunctionType`](task/2/common/asg.hpp:117) 未实现

### 实现要点

**a) 基础类型映射**

```
Type::Spec::kVoid     → llvm::Type::getVoidTy(mCtx)
Type::Spec::kChar     → llvm::Type::getInt8Ty(mCtx)
Type::Spec::kInt      → llvm::Type::getInt32Ty(mCtx)  [已实现]
Type::Spec::kLong     → llvm::Type::getInt64Ty(mCtx)
Type::Spec::kLongLong → llvm::Type::getInt64Ty(mCtx)
```

**b) 指针类型**

LLVM API 范式（LLVM 15.0 Programmer's Manual, Chapter 3 "Type Class Hierarchy"）:

```cpp
// 递归获取基类型
auto baseTy = self(&subt);
// 创建指针类型
return llvm::PointerType::get(baseTy, 0);
```

**c) 数组类型**

```cpp
auto elemTy = self(&subt);
auto len = p->len; // ArrayType::len
return llvm::ArrayType::get(elemTy, len);
```

**d) 函数类型**

```cpp
auto retTy = self(&subt);
std::vector<llvm::Type*> pty;
for (auto paramTy : p->params)
  pty.push_back(self(paramTy));
return llvm::FunctionType::get(retTy, pty, false); // 无 vararg
```

### 涉及的 asg.hpp 节点
- [`Type`](task/2/common/asg.hpp:17) — `spec`, `qual`, `texp`
- [`PointerType`](task/2/common/asg.hpp:100) — `qual`, `sub`
- [`ArrayType`](task/2/common/asg.hpp:108) — `len`, `sub`
- [`FunctionType`](task/2/common/asg.hpp:117) — `params`, `sub`

---

## 阶段二：表达式系统

### 目标
让 [`EmitIR::operator()(Expr*)`](task/3/EmitIR.cpp:64) 支持所有表达式类型。

### 2a. DeclRefExpr — 变量引用

`obj->decl->any` 存储了 `llvm::AllocaInst*` 或 `llvm::GlobalVariable*`。

```cpp
auto val = obj->decl->any; // 这是 alloca/global var 指针
// 注意：此时产生的是左值（地址），类型为该变量的指针类型
return val; // 返回的是 llvm::Value* 即指针
```

**重要**：`DeclRefExpr` 返回的是变量地址（左值），需要结合后续的 `ImplicitCastExpr::kLValueToRValue` 来做 load。

### 2b. ImplicitCastExpr — 隐式类型转换

这是最重要的表达式节点之一，必须正确实现四种 cast。

```cpp
auto subVal = self(p->sub);

switch (p->kind) {
case ImplicitCastExpr::kLValueToRValue:
  // 从 alloca 或 global 中 load 出值
  return mCurIrb->CreateLoad(self(p->sub->type), subVal);

case ImplicitCastExpr::kIntegralCast: {
  auto dstTy = self(p->type);
  auto srcTy = subVal->getType();
  if (srcTy == dstTy) return subVal;
  // 目标更宽 → zext (无符号)/sext (有符号)
  // 目标更窄 → trunc
  // 参考 Typing::promote_integer — 总是从小类型提升到大类型
  if (dstTy->isIntegerTy() && srcTy->isIntegerTy()) {
    auto srcBits = srcTy->getIntegerBitWidth();
    auto dstBits = dstTy->getIntegerBitWidth();
    if (dstBits > srcBits)
      return mCurIrb->CreateSExt(subVal, dstTy);
    else
      return mCurIrb->CreateTrunc(subVal, dstTy);
  }
  ABORT();
}

case ImplicitCastExpr::kArrayToPointerDecay: {
  // 数组退化为指向首元素的指针
  // subVal 是 alloca 地址或全局变量地址，需先算出首元素地址
  return mCurIrb->CreateGEP(arrElemTy, subVal, {zero32, zero32});
}

case ImplicitCastExpr::kFunctionToPointerDecay:
  // 直接返回，函数指针在 LLVM 中与函数值同义
  return subVal;

case ImplicitCastExpr::kNoOp:
  return subVal;
}
```

### 2c. ParenExpr

直接处理 sub 表达式：

```cpp
return self(p->sub);
```

### 2d. UnaryExpr

| op | LLVM 指令 |
|----|----------|
| `kPos` | 直接透传 subVal |
| `kNeg` | `irb.CreateNeg(subVal)` |
| `kNot` | `irb.CreateNot(subVal)` 或 `irb.CreateICmpEQ(subVal, zero)` |

注意：Typing 阶段已将 sub 转换为 rvalue 并做了整数提升。

### 2e. BinaryExpr

| op | LLVM 指令 | 说明 |
|----|----------|------|
| `kAdd` | `irb.CreateAdd(l, r)` | |
| `kSub` | `irb.CreateSub(l, r)` | |
| `kMul` | `irb.CreateMul(l, r)` | |
| `kDiv` | `irb.CreateSDiv(l, r)` | 有符号除法 |
| `kMod` | `irb.CreateSRem(l, r)` | 有符号取模 |
| `kGt` | `irb.CreateICmpSGT(l, r)` | 有符号大于 |
| `kLt` | `irb.CreateICmpSLT(l, r)` | 有符号小于 |
| `kGe` | `irb.CreateICmpSGE(l, r)` | |
| `kLe` | `irb.CreateICmpSLE(l, r)` | |
| `kEq` | `irb.CreateICmpEQ(l, r)` | |
| `kNe` | `irb.CreateICmpNE(l, r)` | |
| `kAssign` | `irb.CreateStore(rhtVal, lftAddr)` | 需要 lft 保持左值形态 |
| `kComma` | 先求值 lft，再返回 rht | |
| `kIndex` | `irb.CreateGEP(lft, {rht})` | 数组/指针索引 |

**Assign 特殊处理**：二元表达式的 `kAssign` 需要左操作数保持地址形态（不对 lft 做 LValueToRValue），只对右操作数做 load。Typing 阶段 LValueToRValue 节点被插入在赋值节点两侧之外，所以遍历时遇到 `BinaryExpr::kAssign` 需：

```cpp
case BinaryExpr::kAssign: {
  auto lftAddr = self(p->lft); // 左值，保持地址
  auto rhtVal = self(p->rht);  // 右值，已被 ImplicitCastExpr 转为 rvalue
  mCurIrb->CreateStore(rhtVal, lftAddr);
  return rhtVal; // C 赋值表达式的结果是右值
}
```

### 2f. StringLiteral

创建全局字符串常量：

```cpp
auto& irb = *mCurIrb;
return irb.CreateGlobalStringPtr(obj->val);
```

---

## 阶段三：局部变量声明

### 目标
区分全局/局部 VarDecl，支持各种初始化表达式。

### 3a. 全局 vs 局部判断

通过检查 `mCurFunc` 是否为空：

```cpp
void EmitIR::operator()(VarDecl* obj) {

  if (mCurFunc == nullptr) {
    // === 全局变量 ===（现有逻辑保持，但需要修复类型硬编码问题）
    auto ty = self(obj->type); // 不要硬编码 i32！
    auto gvar = new llvm::GlobalVariable(mMod, ty, obj->type->qual.const_,
      llvm::GlobalVariable::ExternalLinkage,
      /*initializer=*/nullptr, obj->name);
    obj->any = gvar;
    // ... 构造函数初始化逻辑
  } else {
    // === 局部变量 ===
    auto& irb = *mCurIrb;
    auto ty = self(obj->type);
    auto alloc = irb.CreateAlloca(ty, nullptr, obj->name);
    obj->any = alloc;

    if (obj->init)
      trans_init(alloc, obj->init);
  }
}
```

### 3b. 初始化表达式增强

当前 [`trans_init`](task/3/EmitIR.cpp:144) 仅处理 `IntegerLiteral`，需改为通用的递归调用：

```cpp
void EmitIR::trans_init(llvm::Value* val, Expr* obj) {
  auto initVal = self(obj); // 递归调用表达式求值
  mCurIrb->CreateStore(initVal, val);
}
```

### 3c. DeclStmt — 声明语句

```cpp
void EmitIR::operator()(DeclStmt* obj) {
  for (auto&& decl : obj->decls)
    self(decl);
}
```

---

## 阶段四：控制流语句

### 目标
支持 [`IfStmt`](task/2/common/asg.hpp:297), [`WhileStmt`](task/2/common/asg.hpp:306), [`BreakStmt`](task/2/common/asg.hpp:324), [`ContinueStmt`](task/2/common/asg.hpp:332)。

### 4a. 数据结构增强

在 [`EmitIR.hpp`](task/3/EmitIR.hpp) 中添加：

```cpp
// 循环块记录，用于 break/continue
struct LoopBlocks {
  llvm::BasicBlock* condBb;  // continue 跳转到此处
  llvm::BasicBlock* endBb;   // break 跳转到此处
};
std::vector<LoopBlocks> mLoopStack;
```

### 4b. IfStmt

```
         ┌──────────┐
         │  condBB   │ ← entry 的 fallthrough
         │  cond =   │
         │  self(cond)│
         └────┬─────┘
              │
    ┌─────────┴─────────┐
    │ condBr             │
    ▼                    ▼
┌───────┐          ┌────────┐
│thenBB │          │ elseBB │ (可为 null)
│ ...   │          │ ...    │
└───┬───┘          └───┬────┘
    │                  │
    │  br endBB        │  br endBB
    └────────┬─────────┘
             ▼
         ┌───────┐
         │ endBB │ (merge block)
         └───────┘
```

LLVM API 范式：

```cpp
void EmitIR::operator()(IfStmt* obj) {
  auto& irb = *mCurIrb;
  auto func = mCurFunc;

  auto thenBb = llvm::BasicBlock::Create(mCtx, "if_then", func);
  auto elseBb = obj->else_ ? llvm::BasicBlock::Create(mCtx, "if_else", func) : nullptr;
  auto endBb = llvm::BasicBlock::Create(mCtx, "if_end", func);

  // 条件求值
  auto cond = self(obj->cond);
  // cond 是 i1？SysY C 中关系运算返回 int，需要 IR 层转为 i1：
  auto condI1 = irb.CreateICmpNE(cond, llvm::ConstantInt::get(cond->getType(), 0));
  irb.CreateCondBr(condI1, thenBb, elseBb ? elseBb : endBb);

  // then 分支
  irb.SetInsertPoint(thenBb);
  self(obj->then);
  if (!thenBb->getTerminator())
    irb.CreateBr(endBb);

  // else 分支
  if (elseBb) {
    irb.SetInsertPoint(elseBb);
    self(obj->else_);
    if (!elseBb->getTerminator())
      irb.CreateBr(endBb);
  }

  // 继续在 endBb
  irb.SetInsertPoint(endBb);
}
```

### 4c. WhileStmt

```
         ┌──────────┐
         │  condBB   │ ← 循环条件判断
    ┌───►│  cond =   │
    │    │  self(cond)│
    │    └────┬─────┘
    │         │
    │   condBr│
    │    ┌────┴────┐
    │    ▼         ▼
    │ ┌──────┐  ┌──────┐
    │ │bodyBB│  │endBB │
    │ │ ...  │  └──────┘
    │ │ br   │──┐
    │ └──────┘  │
    └───────────┘
```

```cpp
void EmitIR::operator()(WhileStmt* obj) {
  auto& irb = *mCurIrb;
  auto func = mCurFunc;

  auto condBb = llvm::BasicBlock::Create(mCtx, "while_cond", func);
  auto bodyBb = llvm::BasicBlock::Create(mCtx, "while_body", func);
  auto endBb = llvm::BasicBlock::Create(mCtx, "while_end", func);

  // 压入循环块栈
  mLoopStack.push_back({condBb, endBb});

  irb.CreateBr(condBb);
  irb.SetInsertPoint(condBb);
  auto cond = self(obj->cond);
  auto condI1 = irb.CreateICmpNE(cond,
    llvm::ConstantInt::get(cond->getType(), 0));
  irb.CreateCondBr(condI1, bodyBb, endBb);

  irb.SetInsertPoint(bodyBb);
  self(obj->body);
  if (!bodyBb->getTerminator())
    irb.CreateBr(condBb);

  irb.SetInsertPoint(endBb);

  // 弹出循环块栈
  mLoopStack.pop_back();
}
```

### 4d. BreakStmt / ContinueStmt

```cpp
void EmitIR::operator()(BreakStmt* obj) {
  // break 跳转到最近循环的 endBb
  auto& loopInfo = mLoopStack.back();
  mCurIrb->CreateBr(loopInfo.endBb);
  // 创建新 BB 防止后续指令插入到 unreachable 区域
  auto afterBb = llvm::BasicBlock::Create(mCtx, "after_break", mCurFunc);
  mCurIrb->SetInsertPoint(afterBb);
}

void EmitIR::operator()(ContinueStmt* obj) {
  auto& loopInfo = mLoopStack.back();
  mCurIrb->CreateBr(loopInfo.condBb);
  auto afterBb = llvm::BasicBlock::Create(mCtx, "after_continue", mCurFunc);
  mCurIrb->SetInsertPoint(afterBb);
}
```

### 4e. ReturnStmt 修复

当前 `ReturnStmt` 在 CreateRet 后插入 `return_exit` 块，但这是多余且可能引起问题的。建议简化为：

```cpp
void EmitIR::operator()(ReturnStmt* obj) {
  if (obj->expr) {
    auto retVal = self(obj->expr);
    mCurIrb->CreateRet(retVal);
  } else {
    mCurIrb->CreateRetVoid();
  }
  // 创建 dummy block 防止后续指令污染
  auto exitBb = llvm::BasicBlock::Create(mCtx, "return_exit", mCurFunc);
  mCurIrb->SetInsertPoint(exitBb);
}
```

---

## 阶段五：函数调用

### 目标
处理 [`CallExpr`](task/2/common/asg.hpp:221)。

### 实现

```cpp
llvm::Value* EmitIR::operator()(CallExpr* obj) {
  auto calleeVal = self(obj->head); // 会经过 FunctionToPointerDecay
  auto callee = llvm::dyn_cast<llvm::Function>(calleeVal);
  // 或通过函数指针调用（简单情况用 Function）

  std::vector<llvm::Value*> args;
  for (auto&& arg : obj->args)
    args.push_back(self(arg));

  return mCurIrb->CreateCall(callee->getFunctionType(), calleeVal, args);
}
```

### 函数参数

在 [`FunctionDecl`](task/3/EmitIR.cpp:187) 中添加参数 alloca：

```cpp
// 在 entry BB 中设置参数
for (auto&& arg : func->args()) {
  auto param = obj->params[arg.getArgNo()];
  auto alloc = entryIrb.CreateAlloca(arg.getType(), nullptr, param->name);
  entryIrb.CreateStore(&arg, alloc);
  param->any = alloc;
}
```

### 外部函数声明

当前 `FunctionDecl` 对 `body == nullptr` 仅设置了 `obj->any` 后直接返回。需要处理库函数（如 `putint`、`getint` 等）：

```cpp
// 在 mMod 中声明外部函数
// 已有 FunctionDecl 创建逻辑，设 ExternalLinkage 即可
```

---

## 阶段六：剩余语句

### 6a. NullStmt — 空语句

```cpp
void EmitIR::operator()(NullStmt* obj) {
  // 什么都不做
}
```

### 6b. ExprStmt — 表达式语句

```cpp
void EmitIR::operator()(ExprStmt* obj) {
  self(obj->expr); // 丢弃结果
}
```

### 6c. DoStmt — do-while 循环

与 WhileStmt 类似，但 body 先于 cond 执行一次。

---

## 阶段七：短路求值

### 目标
`BinaryExpr::kAnd` 和 `BinaryExpr::kOr` 的短路语义。

```
// A && B 的 IR 结构：
//   condA = evaluate A
//   br condA ? evalB : false_end
// evalB:
//   condB = evaluate B
//   br true_end (with condB)
// false_end:
//   br true_end (with false)
// true_end:
//   phi [condB, evalB], [false, false_end]
```

具体实现留待阶段七细化。

---

## 阶段八：调试策略

### 调试工作流

```bash
# 1. 单测例运行
cd build && cmake .. && make -j$(nproc)
cd build/test/task3
ctest --test-dir . -R "functional-0/001"

# 2. 查看生成的 IR
cat functional-0/001_var_defn.sysu.c/output.ll

# 3. 手动验证 IR
opt -verify < functional-0/001_var_defn.sysu.c/output.ll

# 4. 用 lli 执行
lli functional-0/001_var_defn.sysu.c/output.ll; echo $?

# 5. 对比标准答案
diff <(lli functional-0/001_var_defn.sysu.c/output.ll 2>&1) \
     <(cat functional-0/001_var_defn.sysu.c/answer.out)
```

### 常见问题排查

| 现象 | 可能原因 | 检查点 |
|------|---------|--------|
| ABORT | 未实现的 ASG 节点 | 查看 stderr 中的 `aborted at` 行号 |
| IR verify 失败 | 基本块未终结/invalid type | `opt -verify` |
| lli 段错误 | alloca 地址未正确传递 | 检查 DeclRefExpr 的 any 字段 |
| 返回值不符 | 表达式求值顺序或类型转换错误 | 对比 answer.ll |
| 输出不符 | putint 调用问题或 stdout 未 flush | 对比 answer.out |

---

## 实现顺序建议

按测试用例依赖关系，推荐以下优先级：

1. **P0 — 类型系统**（阶段一）：所有后续阶段依赖
2. **P1 — 表达式 + 局部变量**（阶段二 + 三）：支撑 functional-0 全部用例
3. **P2 — 控制流**（阶段四）：支撑 functional-3 的 if/while 用例
4. **P3 — 函数调用**（阶段五）：支撑 functional-2 的 func_defn
5. **P4 — 剩余语句 + 短路求值**（阶段六 + 七）：支撑边界用例