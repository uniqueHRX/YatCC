#include "LICM.hpp"

using namespace llvm;

PreservedAnalyses
LICM::run(Module& mod, ModuleAnalysisManager& mam)
{
  int hoistTimes = 0;

  // 遍历模块中的每个函数
  for (Function& func : mod) {
    if (func.isDeclaration()) continue;
    // 获取函数的循环分析结果
    DominatorTree DT(func);
    LoopInfo LI(DT);
    // 由内向外遍历函数中的每个循环
    for (auto* loop : LI.getLoopsInPreorder()) {
      auto preheader = loop->getLoopPreheader();
      // 如果循环没有前置块，则无法进行 LICM
      if (!preheader) continue;
      std::vector<Instruction*> toHoist;
      // 迭代提升循环无关的指令
      do {
        toHoist.clear();
        // 获取循环的基本块列表
        for (auto* bb : loop->getBlocks()) {
          // 遍历循环基本块中的每条指令
          for (Instruction& inst : *bb) {
            // 当前指令不能是终止指令
            if (inst.isTerminator()) continue;
            // 判断当前指令是否有副作用或者内存访问
            if (inst.mayHaveSideEffects() || inst.mayReadOrWriteMemory()) continue;
             // 判断当前指令是否依赖于循环内定义的值
            bool dependsOnLoop = false;
            for (Use& operand : inst.operands()) {
              if (!loop->isLoopInvariant(operand.get())) {
                dependsOnLoop = true;
                break;
              }
            }
            // 如果指令不依赖于循环内定义的值，则可以提升到循环前置块中
            if (!dependsOnLoop) {
              toHoist.push_back(&inst);
              ++hoistTimes; 
            }
          }
        }
        // 将循环无关的指令提升到循环前置块中
        for (Instruction* inst : toHoist) {
          auto insertPoint = preheader->getTerminator();
          inst->moveBefore(insertPoint);
        }
      }
      // 如果在本次迭代中有指令被提升，则需要继续检查是否还有其他指令可以被提升
      while (!toHoist.empty());
    }
  }
  mOut << "LICM running...\nTo hoist " << hoistTimes
  << " instructions\n";
  return PreservedAnalyses::all();
}