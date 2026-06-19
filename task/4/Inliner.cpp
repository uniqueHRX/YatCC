#include "Inliner.hpp"

using namespace llvm;

// 定义工作列表项结构体
struct WorkItem
{
  CallInst* call;
  Function* callee;
};

PreservedAnalyses
Inliner::run(Module& mod, ModuleAnalysisManager& mam)
{
  int inlinedTimes = 0;

  // 通过工作列表收集所有可内联的调用指令和对应的被调用函数
  std::vector<WorkItem> worklist;
  // 遍历模块中的每个函数
  for (Function& func : mod) {
    if (func.isDeclaration()) continue;
    // 遍历函数中的每个基本块
    for (BasicBlock& bb : func) {
      // 遍历基本块中的每条指令，寻找可内联的调用指令
      for (Instruction& inst : bb) {
        if (auto* callInst = dyn_cast<CallInst>(&inst)) {
          // 只处理满足约束条件的函数调用，以避免内联过于复杂或不合适的函数
          auto* callee = callInst->getCalledFunction();
          if (!callee || callee->isDeclaration()) continue;
          if (callee->isVarArg()) continue;
          if (callee == &func) continue;
          if (callee->getInstructionCount() > 10000) continue;
          if (callee->size() != 1) continue;
          // 将满足条件的调用指令和对应的被调用函数添加到工作列表中
          worklist.push_back({ callInst, callee });
        }
      }
    }
  }

  // 处理工作列表中的每个调用指令，执行内联操作
  for (auto& item : worklist) {
    auto* callInst = item.call;
    auto* callee = item.callee;
    if (callee->getParent() == nullptr) continue;
    // 获取函数调用相关信息
    Function* caller = callInst->getFunction();
    BasicBlock& entryBB = callee->getEntryBlock();
    DenseMap<Value*, Value*> VMap;
    // 映射实参到形参
    for (unsigned i = 0; i < callee->arg_size(); ++i)
      VMap[callee->getArg(i)] = callInst->getArgOperand(i);
    // 将插入点设置函数调用点前
    IRBuilder<> builder(callInst);
    Value* retVal = nullptr;
    // 遍历被调用函数入口块中的每条指令，进行克隆和映射
    for (Instruction& inst : entryBB) {
      // 处理返回指令，记录返回值
      if (auto* ret = dyn_cast<ReturnInst>(&inst)) {
        Value* rv = ret->getReturnValue();
        if (rv) {
          retVal = VMap.lookup(rv);
          if (!retVal) retVal = rv;
        }
      }
      // 处理 alloca 指令，将其克隆到调用函数的入口块中以确保正确的内存分配位置
      else if (isa<AllocaInst>(&inst)) {
        auto* cloned = inst.clone();
        IRBuilder<> entryBuilder(&caller->getEntryBlock().back());
        entryBuilder.Insert(cloned);
        VMap[&inst] = cloned;
      }
      // 处理其他指令，进行克隆并重映射操作数
      else {
        auto* cloned = inst.clone();
        for (unsigned i = 0; i < cloned->getNumOperands(); ++i) {
          if (auto* mapped = VMap.lookup(cloned->getOperand(i)))
            cloned->setOperand(i, mapped);
        }
        builder.Insert(cloned);
        VMap[&inst] = cloned;
      }
    }
    // 替换调用指令的使用者为返回值，并删除调用指令
    if (retVal)
      callInst->replaceAllUsesWith(retVal);
    callInst->eraseFromParent();
    ++inlinedTimes;
  }

  mOut << "Inliner running...\nInlined " << inlinedTimes
       << " calls\n";
  return PreservedAnalyses::all();
}