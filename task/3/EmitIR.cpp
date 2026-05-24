#include "EmitIR.hpp"
#include <llvm/Transforms/Utils/ModuleUtils.h>

#define self (*this)

using namespace asg;

EmitIR::EmitIR(Obj::Mgr& mgr, llvm::LLVMContext& ctx, llvm::StringRef mid)
  : mMgr(mgr)
  , mMod(mid, ctx)
  , mCtx(ctx)
  , mIntTy(llvm::Type::getInt32Ty(ctx))
  , mCurIrb(std::make_unique<llvm::IRBuilder<>>(ctx))
  , mCtorTy(llvm::FunctionType::get(llvm::Type::getVoidTy(ctx), false))
{
}

llvm::Module&
EmitIR::operator()(asg::TranslationUnit* tu)
{
  for (auto&& i : tu->decls)
    self(i);
  return mMod;
}

//==============================================================================
// 类型
//==============================================================================

llvm::Type*
EmitIR::operator()(const Type* type)
{
  if (type->texp == nullptr) {
    switch (type->spec) {
      case Type::Spec::kInt:
        return llvm::Type::getInt32Ty(mCtx);
      // TODO: 在此添加对更多基础类型的处理
      default:
        ABORT();
    }
  }

  Type subt;
  subt.spec = type->spec;
  subt.qual = type->qual;
  subt.texp = type->texp->sub;

  // TODO: 在此添加对指针类型、数组类型和函数类型的处理
  if (auto p = type->texp->dcst<PointerType>()) {
    return llvm::PointerType::get(self(&subt), 0);
  }

  if (auto p = type->texp->dcst<ArrayType>()) {
    auto len = p->len;
    return llvm::ArrayType::get(self(&subt), len);
  }

  if (auto p = type->texp->dcst<FunctionType>()) {
    std::vector<llvm::Type*> pty;
    // TODO: 在此添加对函数参数类型的处理
    if (!p->params.empty())
       for (auto&& param : p->params)
         pty.push_back(self(param));

    return llvm::FunctionType::get(self(&subt), std::move(pty), false);
  }

  ABORT();
}

//==============================================================================
// 表达式
//==============================================================================

llvm::Value*
EmitIR::operator()(Expr* obj)
{
  // TODO: 在此添加对更多表达式处理的跳转
  if (auto p = obj->dcst<IntegerLiteral>())
    return self(p);

  if (auto p = obj->dcst<ParenExpr>())
    return self(p);

  if (auto p = obj->dcst<UnaryExpr>())
    return self(p);

  if (auto p = obj->dcst<BinaryExpr>())
    return self(p);

  if (auto p = obj->dcst<CallExpr>())
    return self(p);

  if (auto p = obj->dcst<ImplicitInitExpr>())
    return self(p);

  if (auto p = obj->dcst<ImplicitCastExpr>())
    return self(p);

  if (auto p = obj->dcst<DeclRefExpr>())
    return self(p);

  ABORT();
}

llvm::Constant*
EmitIR::operator()(IntegerLiteral* obj)
{
  return llvm::ConstantInt::get(self(obj->type), obj->val);
}

// TODO: 在此添加对更多表达式类型的处理

// 括号表达式
llvm::Value*
EmitIR::operator()(ParenExpr* obj)
{
  return self(obj->sub);
}

// 一元表达式
llvm::Value*
EmitIR::operator()(UnaryExpr* obj)
{
  llvm::Value* sub = self(obj->sub);

  auto& irb = *mCurIrb;

  switch (obj->op) {
    case UnaryExpr::Op::kPos:
      return sub;

    case UnaryExpr::Op::kNeg:
      return irb.CreateNeg(sub);

    case UnaryExpr::Op::kNot:
      return irb.CreateICmpEQ(sub, irb.getInt32(0));

    default:
      ABORT();
  }
}

// 二元表达式
llvm::Value*
EmitIR::operator()(asg::BinaryExpr* obj)
{
  llvm::Value *lftVal = self(obj->lft);
  llvm::Value *rhtVal = self(obj->rht);

  auto& irb = *mCurIrb;

  switch (obj->op) {
    case BinaryExpr::Op::kAdd:
      return irb.CreateAdd(lftVal, rhtVal);

    case BinaryExpr::Op::kSub:
      return irb.CreateSub(lftVal, rhtVal);

    case BinaryExpr::Op::kMul:
      return irb.CreateMul(lftVal, rhtVal);

    case BinaryExpr::Op::kDiv:
      return irb.CreateSDiv(lftVal, rhtVal);

    case BinaryExpr::Op::kMod:
      return irb.CreateSRem(lftVal, rhtVal);

    case BinaryExpr::Op::kGt:
      return irb.CreateICmpSGT(lftVal, rhtVal);
    
    case BinaryExpr::Op::kLt:
      return irb.CreateICmpSLT(lftVal, rhtVal);

    case BinaryExpr::Op::kGe:
      return irb.CreateICmpSGE(lftVal, rhtVal);

    case BinaryExpr::Op::kLe:
      return irb.CreateICmpSLE(lftVal, rhtVal);

    case BinaryExpr::Op::kEq:
      return irb.CreateICmpEQ(lftVal, rhtVal);

    case BinaryExpr::Op::kNe:
      return irb.CreateICmpNE(lftVal, rhtVal);

    case BinaryExpr::Op::kAnd:
      return irb.CreateAnd(lftVal, rhtVal);

    case BinaryExpr::Op::kOr:
      return irb.CreateOr(lftVal, rhtVal);

    case BinaryExpr::Op::kAssign:
      irb.CreateStore(rhtVal, lftVal);
      return rhtVal;

    case BinaryExpr::Op::kComma:
      return rhtVal;

    case BinaryExpr::Op::kIndex: {
      auto p = obj->lft->dcst<ImplicitCastExpr>()->sub;
      auto ty = self(p->type);
      auto gep = irb.CreateInBoundsGEP(ty, lftVal, {irb.getInt64(0), rhtVal});
      return gep;
    }

    default:
      ABORT();
  }
}

// 函数调用表达式
llvm::Value*
EmitIR::operator()(CallExpr* obj)
{
  auto& irb = *mCurIrb;

  // 直接通过dyn_cast拿到被调用函数的llvm::Function对象
  auto func = llvm::dyn_cast<llvm::Function>(self(obj->head));

  // 处理函数参数
  std::vector<llvm::Value*> args;
  for (auto&& arg : obj->args)
    args.push_back(self(arg));

  return irb.CreateCall(func, args);
}

// // 隐式空初始化表达式
// llvm::Value*
// EmitIR::operator()(asg::ImplicitInitExpr* obj)
// {
//   auto ty = self(obj->type);

//   if (ty->isAggregateType())
//     return llvm::Constant::getNullValue(ty);

//   return llvm::ConstantInt::get(ty, 0);
// }

// 隐式类型转换表达式
llvm::Value*
EmitIR::operator()(asg::ImplicitCastExpr* obj)
{
  auto sub = self(obj->sub);

  auto& irb = *mCurIrb;

  switch (obj->kind) {
    // LtoR 值转换
    case ImplicitCastExpr::kLValueToRValue: {
      auto ty = self(obj->sub->type);
      return irb.CreateLoad(ty, sub);
    }

    // 数组到指针转换
    case ImplicitCastExpr::kArrayToPointerDecay: {
      // sub 是被 decay 的数组地址（DeclRefExpr → ImplicitCastExpr 链底）
      // 直接穿透到最内层非 cast 节点拿类型
      auto inner = obj->sub;
      while (auto cast = inner->dcst<ImplicitCastExpr>())
        inner = cast->sub;
      auto ty = self(inner->type);
      return irb.CreateInBoundsGEP(ty, sub, {irb.getInt64(0), irb.getInt64(0)});
    }

    case ImplicitCastExpr::kFunctionToPointerDecay:
      return sub;

    case ImplicitCastExpr::kNoOp:
      return sub;

    default:
      ABORT();
  }
}

llvm::Value*
EmitIR::operator()(asg::DeclRefExpr* obj)
{
  // 在LLVM IR层面，左值体现为返回指向值的指针
  // 在ImplicitCastExpr::kLValueToRValue中发射load指令从而变成右值
  return reinterpret_cast<llvm::Value*>(obj->decl->any);
}

//==============================================================================
// 语句
//==============================================================================

void
EmitIR::operator()(Stmt* obj)
{
  // TODO: 在此添加对更多Stmt类型的处理的跳转

  if (auto p = obj->dcst<CompoundStmt>())
    return self(p);

  if (auto p = obj->dcst<ReturnStmt>())
    return self(p);

  if (auto p = obj->dcst<DeclStmt>())
    return self(p);

  if (auto p = obj->dcst<ExprStmt>())
    return self(p);

  ABORT();
}

void
EmitIR::operator()(CompoundStmt* obj)
{
  // TODO: 可以在此添加对符号重名的处理
  for (auto&& stmt : obj->subs)
    self(stmt);
}

void
EmitIR::operator()(ReturnStmt* obj)
{
  auto& irb = *mCurIrb;

  llvm::Value* retVal;
  if (!obj->expr)
    retVal = nullptr;
  else
    retVal = self(obj->expr);

  mCurIrb->CreateRet(retVal);

  auto exitBb = llvm::BasicBlock::Create(mCtx, "return_exit", mCurFunc);
  mCurIrb->SetInsertPoint(exitBb);
}

// TODO: 在此添加对更多Stmt类型的处理
void
EmitIR::operator()(DeclStmt* obj)
{
  for (auto&& decl : obj->decls)
    self(decl);
}

void
EmitIR::operator()(ExprStmt* obj)
{
  self(obj->expr);
}

//==============================================================================
// 声明
//==============================================================================

void
EmitIR::operator()(Decl* obj)
{
  // TODO: 添加变量声明处理的跳转
  if (auto p = obj->dcst<VarDecl>())
    return self(p);

  if (auto p = obj->dcst<FunctionDecl>())
    return self(p);

  ABORT();
}

// TODO: 添加变量声明的处理

// 变量初始化方法
void
EmitIR::trans_init(llvm::Value* val, Expr* obj, llvm::Type* initTy)
{
  auto& irb = *mCurIrb;

  // 处理整数字面量的初始化
  if (auto p = obj->dcst<IntegerLiteral>()) {
    auto initVal = llvm::ConstantInt::get(self(p->type), p->val);
    irb.CreateStore(initVal, val);
    return;
  }

  // 处理变量引用的初始化
  if (auto p = obj->dcst<DeclRefExpr>()) {
    auto initVal = irb.CreateLoad(self(p->decl->type), reinterpret_cast<llvm::Value*>(p->decl->any));
    irb.CreateStore(initVal, val);
    return;
  }

  // 处理函数调用的初始化
  if (auto p = obj->dcst<CallExpr>()) {
    auto initVal = self(p);
    irb.CreateStore(initVal, val);
    return;
  }

  // 处理隐式零初始化
  if (auto p = obj->dcst<ImplicitInitExpr>()) {
    if (initTy->isAggregateType()) {
      auto zero = llvm::Constant::getNullValue(initTy);
      irb.CreateStore(zero, val);
    }
    return;
  }

  // 处理隐式类型转换的初始化
  if (auto p = obj->dcst<ImplicitCastExpr>()) {
    auto initVal = self(p);
    irb.CreateStore(initVal, val);
    return;
  }

  // 处理初始化列表的初始化
  if (auto p = obj->dcst<InitListExpr>()) {
    if (!initTy->isArrayTy())
      ABORT();

    auto elemTy = initTy->getArrayElementType();

    for (std::size_t i = 0; i < initTy->getArrayNumElements(); ++i) {
      auto gep = irb.CreateInBoundsGEP(initTy, val, {irb.getInt64(0), irb.getInt64(i)});
      // 有值则初始化，没有值则默认初始化为 0
      if (i < p->list.size())
        trans_init(gep, p->list[i], elemTy);
      else
        irb.CreateStore(llvm::Constant::getNullValue(elemTy), gep);
    }
    return;
  }

  // 如果表达式不是上述类型，则抛出异常
  ABORT();
}

void
EmitIR::operator()(VarDecl* obj)
{
  // 处理全局变量
  if (mCurIrb->GetInsertBlock() == nullptr) {
    auto ty = self(obj->type);
    auto gvar = new llvm::GlobalVariable(
      mMod, ty, false, llvm::GlobalVariable::ExternalLinkage, nullptr, obj->name
    );
    obj->any = gvar;

    // 默认初始化为 0
    if (ty->isAggregateType())
      gvar->setInitializer(llvm::Constant::getNullValue(ty));
    else
      gvar->setInitializer(llvm::ConstantInt::get(ty, 0));

    if (obj->init == nullptr)
      return;

    // 保存当前函数和基本块
    auto savedFunc = mCurFunc;
    auto savedBb = mCurIrb->GetInsertBlock();

    // 创建构造函数用于初始化
    mCurFunc = llvm::Function::Create(
      mCtorTy, llvm::GlobalVariable::PrivateLinkage, "ctor_" + obj->name, mMod);
    llvm::appendToGlobalCtors(mMod, mCurFunc, 65535);
    
    auto entryBb = llvm::BasicBlock::Create(mCtx, "entry", mCurFunc);
    mCurIrb->SetInsertPoint(entryBb);
    trans_init(gvar, obj->init, ty);
    mCurIrb->CreateRet(nullptr);

    // 恢复当前函数和基本块
    mCurFunc = savedFunc;
    mCurIrb->SetInsertPoint(savedBb);
  }

  // 处理局部变量
  else {
    auto& irb = *mCurIrb;
    auto ty = self(obj->type);
    auto lvar = irb.CreateAlloca(ty, nullptr, obj->name);
    obj->any = lvar;

    if (obj->init == nullptr)
      return;

    trans_init(lvar, obj->init, ty);
  }

  return;
}

void
EmitIR::operator()(FunctionDecl* obj)
{
  // 创建函数
  auto fty = llvm::dyn_cast<llvm::FunctionType>(self(obj->type));
  auto func = llvm::Function::Create(
    fty, llvm::GlobalVariable::ExternalLinkage, obj->name, mMod);

  obj->any = func;

  if (obj->body == nullptr)
    return;
  auto entryBb = llvm::BasicBlock::Create(mCtx, "entry", func);
  mCurIrb->SetInsertPoint(entryBb);
  auto& entryIrb = *mCurIrb;

  // TODO: 添加对函数参数的处理
  for (std::size_t i = 0; i < fty->getNumParams(); ++i) {
    auto param = obj->params[i];
    auto arg = func->getArg(i);
    arg->setName(param->name);
    auto alloca = entryIrb.CreateAlloca(arg->getType(), nullptr, param->name);
    entryIrb.CreateStore(arg, alloca);
    param->any = alloca;
  }

  // 翻译函数体
  mCurFunc = func;
  self(obj->body);
  auto& exitIrb = *mCurIrb;

  if (fty->getReturnType()->isVoidTy())
    exitIrb.CreateRetVoid();
  else
    exitIrb.CreateUnreachable();
}
