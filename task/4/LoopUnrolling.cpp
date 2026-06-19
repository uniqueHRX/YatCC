#include "LoopUnrolling.hpp"

using namespace llvm;

PreservedAnalyses
LoopUnrolling::run(Module& mod, ModuleAnalysisManager& mam)
{
  int unrollTimes = 0;

  // 遍历模块中的每个函数
  for (Function& func : mod) {
    if (func.isDeclaration()) continue;
    // 获取循环分析信息
    DominatorTree DT(func);
    LoopInfo LI(DT);
    // 由内向外遍历函数中的每个循环
    for (auto* loop : LI.getLoopsInPreorder()) {
      // 获取循环的块信息
      auto* preheader = loop->getLoopPreheader();
      auto* header = loop->getHeader();
      auto* latch = loop->getLoopLatch();
      auto* exitBlock = loop->getUniqueExitBlock();
      // 仅处理满足特定结构的简单循环：单一入口、单一出口和单一回边
      if (!preheader || !header || !latch || !exitBlock) continue;
      if (loop->getNumBlocks() != 2) continue;
      if (loop->getExitingBlock() != header) continue;
      if (!exitBlock->phis().empty()) continue;

      // 创建空克隆块与映射表
      DenseMap<Value*, Value*> VMap;
      auto* clonedHeader = BasicBlock::Create(func.getContext(), header->getName() + ".unroll", &func);
      auto* clonedLatch = BasicBlock::Create(func.getContext(), latch->getName() + ".unroll", &func);
      VMap[header] = clonedHeader;
      VMap[latch] = clonedLatch;

      // 克隆 header 和 latch 中的指令到克隆块
      IRBuilder<> hdrBuilder(clonedHeader);
      for (auto& inst : *header) {
        auto* cloned = inst.clone();
        hdrBuilder.Insert(cloned);
        VMap[&inst] = cloned;
      }
      IRBuilder<> latBuilder(clonedLatch);
      for (auto& inst : *latch) {
        auto* cloned = inst.clone();
        latBuilder.Insert(cloned);
        VMap[&inst] = cloned;
      }

      // 重映射克隆指令的操作数，遍历所有克隆块
      for (auto* clonedBB : { clonedHeader, clonedLatch }) {
        // 遍历所有指令
        for (auto& inst : *clonedBB) {
          // 处理PHI指令的操作数重映射，需要同时重映射 value 和 block
          if (auto* phi = dyn_cast<PHINode>(&inst)) {
            for (unsigned i = 0; i < phi->getNumIncomingValues(); ++i) {
              if (auto* mapped = VMap.lookup(phi->getIncomingValue(i)))
                phi->setIncomingValue(i, mapped);
              if (auto* mappedBB = VMap.lookup(phi->getIncomingBlock(i)))
                phi->setIncomingBlock(i, cast<BasicBlock>(mappedBB));
            }
          }
          // 处理其他指令的操作数重映射
          else {
            for (unsigned i = 0; i < inst.getNumOperands(); ++i) {
              if (auto* mapped = VMap.lookup(inst.getOperand(i)))
                inst.setOperand(i, mapped);
            }
          }
        }
      }

      // 重新连接 CFG：
      // 原始 latch → clonedHeader
      auto* latchTerm = latch->getTerminator();
      for (unsigned i = 0; i < latchTerm->getNumSuccessors(); ++i)
        if (latchTerm->getSuccessor(i) == header)
          latchTerm->setSuccessor(i, clonedHeader);
      // clonedLatch → 原始 header
      auto* clonedLatchTerm = clonedLatch->getTerminator();
      for (unsigned i = 0; i < clonedLatchTerm->getNumSuccessors(); ++i)
        if (clonedLatchTerm->getSuccessor(i) == clonedHeader)
          clonedLatchTerm->setSuccessor(i, header);
      // clonedHeader
      auto* clonedHeaderTerm = clonedHeader->getTerminator();
      for (unsigned i = 0; i < clonedHeaderTerm->getNumSuccessors(); ++i) {
        // 继续循环 → clonedLatch
        if (clonedHeaderTerm->getSuccessor(i) == latch)
          clonedHeaderTerm->setSuccessor(i, clonedLatch);
        // 退出 → 原始 header
        else if (clonedHeaderTerm->getSuccessor(i) == exitBlock)
          clonedHeaderTerm->setSuccessor(i, header);
      }

      // 遍历 header 中的 PHI 指令并更新其输入以正确反映新的 CFG 结构
      for (auto& origPhi : header->phis()) {
        // 获取和原始 PHI 对应的克隆 PHI
        auto* clonedPhi = cast<PHINode>(VMap[&origPhi]);
        // 获取 PHI 输入中的块索引
        int origLatchIdx = origPhi.getBasicBlockIndex(latch);
        int clonedPreheaderIdx = clonedPhi->getBasicBlockIndex(preheader);
        int clonedLatchIdx = clonedPhi->getBasicBlockIndex(clonedLatch);

        // 对原始 header PHI，更新来自 latch 的输入为 clonedLatch
        Value* origLatchVal = origLatchIdx >= 0
          ? origPhi.getIncomingValue(origLatchIdx)
          : nullptr;
        if (origLatchIdx >= 0) {
          origPhi.setIncomingBlock(origLatchIdx, clonedLatch);
          origPhi.setIncomingValue(origLatchIdx, VMap[origLatchVal]);
        }
        // 并添加来自 clonedHeader 的输入
        origPhi.addIncoming(clonedPhi, clonedHeader);

        // 对 clonedHeader PHI，更新来自 preheader 的输入为 latch
        if (clonedPreheaderIdx >= 0 && origLatchVal) {
          clonedPhi->setIncomingBlock(clonedPreheaderIdx, latch);
          clonedPhi->setIncomingValue(clonedPreheaderIdx, origLatchVal);
        }
        // 并移除来自 clonedLatch 的 输入
        if (clonedLatchIdx >= 0)
          clonedPhi->removeIncomingValue(clonedLatchIdx);
      }
      ++unrollTimes;
    }
  }

  mOut << "LoopUnrolling running...\nUnrolled " << unrollTimes
       << " loops\n";
  return PreservedAnalyses::none();
}