#include "DCE.hpp"

using namespace llvm;

PreservedAnalyses
DCE::run(Module& mod, ModuleAnalysisManager& mam)
{
  int dceTimes = 0;

  // 迭代删除模块中的死代码，直到没有新的指令被删除
  bool changed;
  do {
    changed = false;
    // 遍历模块中的每个函数
    for (Function& func : mod) {
      // 遍历每个函数的基本块
      for (BasicBlock& bb : func) {
        std::vector<Instruction*> toErase;
        // 遍历每个基本块的指令
        for (Instruction& inst : bb) {
          // 如果当前指令无副作用
          if (!inst.isTerminator() && !inst.mayHaveSideEffects()) {
            // 如果当前指令没有任何使用者，则将其标记为待删除
            if (inst.use_empty()) {
              toErase.push_back(&inst);
              ++dceTimes;
              changed = true;
            } 
          }
        }
        // 统一删除所有被标记为待删除的指令
        for (auto& inst : toErase) {
          inst->eraseFromParent();
        }
      }
    }
  } while (changed);

  mOut << "DCE running...\nTo eliminate " << dceTimes <<
  " instructions\n";
  return PreservedAnalyses::all();
}