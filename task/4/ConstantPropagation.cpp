#include "ConstantPropagation.hpp"

#include <llvm/IR/GlobalVariable.h>

using namespace llvm;

PreservedAnalyses
ConstantPropagation::run(Module& mod, ModuleAnalysisManager& mam)
{
  int constPropTimes = 0;

  // 遍历所有函数
  for (auto& func : mod) {
    // 遍历每个函数的基本块
    for (auto& bb : func) {
      std::vector<Instruction*> instToErase;
      // 遍历每个基本块的指令
      for (auto& inst : bb) {
        // 处理 load 指令
        if (auto* load = dyn_cast<LoadInst>(&inst)) {
          Value* ptr = load->getPointerOperand();
          // 假如 load 的地址来自全局变量且有初始值
          if (auto* GV = dyn_cast<GlobalVariable>(ptr)) {
            if (GV->hasInitializer()) {
              // 检查全局变量是否被 store 修改过
              bool hasStore = false;
              for (User* U : GV->users()) {
                if (isa<StoreInst>(U)) {
                  hasStore = true;
                  break;
                }
              }
              // 该全局变量未被 store 修改过，可以用初始值替换 load
              if (!hasStore) {
                if (auto* constInit = dyn_cast<ConstantInt>(GV->getInitializer())) {
                  load->replaceAllUsesWith(constInit);
                  instToErase.push_back(load);
                  ++constPropTimes;
                }
              }
            }
          }
        }
      }
      // 统一删除被传播替换的 load 指令
      for (auto* i : instToErase)
        i->eraseFromParent();
    }
  }

  mOut << "ConstantPropagation running...\nTo propagate " << constPropTimes
       << " constants\n";
  return PreservedAnalyses::all();
}