#include "DSE.hpp"

using namespace llvm;

PreservedAnalyses
DSE::run(Module& mod, ModuleAnalysisManager& mam)
{
  int dseTimes = 0;

  // 遍历模块中的每个函数
  for (Function& func : mod) {
    // 遍历每个函数的基本块
    for (BasicBlock& bb : func) {
      std::vector<Instruction*> toErase;
      // 遍历每个基本块的指令
      for (Instruction& inst : bb) {
        // 处理 store 指令
        if (auto storeInst = dyn_cast<StoreInst>(&inst)) {
          // 如果 store 指令是 valatile 或 atomic 的，则跳过
          if (storeInst->isVolatile() || storeInst->isAtomic()) {
            continue;
          }
          Value* ptr = storeInst->getPointerOperand();
          // 追踪指针的来源，直到找到非 bitcast 和 getelementptr 的指令
          while (isa<BitCastInst>(ptr) || isa<GetElementPtrInst>(ptr)) {
            ptr = dyn_cast<Instruction>(ptr)->getOperand(0);
          }
          // 检查指针的所有 user，判断是否存在可能受该 store 影响的指令
          bool hasEffect = false;
          for (auto* user : ptr->users()) {
            if (
              isa<LoadInst>(user)
              || isa<CallInst>(user)
              || isa<InvokeInst>(user)
              || isa<BitCastInst>(user)
              || isa<GetElementPtrInst>(user)
            ) {
              hasEffect = true;
              break;
            }
          }
          // 如果 store 指令没有任何影响，则将其标记为待删除
          if (!hasEffect) {
            toErase.push_back(&inst);
            ++dseTimes;
          }
        }
      }
      // 统一删除所有被标记为待删除的指令
      for (auto& inst : toErase) {
        inst->eraseFromParent();
      }
    }
  }
  mOut << "DSE running...\nTo eliminate " << dseTimes <<
  " instructions\n";
  return PreservedAnalyses::all();
}