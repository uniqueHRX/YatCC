#include "LoopUnrolling.hpp"

using namespace llvm;

PreservedAnalyses
LoopUnrolling::run(Module& mod, ModuleAnalysisManager& mam)
{
  // 注册 LoopAnalysis 分析器
  FunctionAnalysisManager fam;
  PassBuilder pb;
  fam.registerPass([&] { return LoopAnalysis(); });
  pb.registerFunctionAnalyses(fam);

  int unrollTimes = 0;

  for (Function& func : mod) {
    if (func.isDeclaration()) continue;
    auto& LI = fam.getResult<LoopAnalysis>(func);

    for (auto* loop : LI.getLoopsInPreorder()) {
      auto* preheader = loop->getLoopPreheader();
      auto* header = loop->getHeader();
      auto* latch = loop->getLoopLatch();
      auto* exitBlock = loop->getUniqueExitBlock();
      if (!preheader || !header || !latch || !exitBlock) continue;

      if (loop->getNumBlocks() != 2) continue;
      if (loop->getExitingBlock() != header) continue;
      if (!exitBlock->phis().empty()) continue;

      // ====== 创建空克隆块 ======
      DenseMap<Value*, Value*> VMap;
      auto* clonedHeader = BasicBlock::Create(
        func.getContext(), header->getName() + ".unroll", &func);
      auto* clonedLatch = BasicBlock::Create(
        func.getContext(), latch->getName() + ".unroll", &func);
      VMap[header] = clonedHeader;
      VMap[latch] = clonedLatch;

      // ====== 克隆指令（不重映射） ======
      IRBuilder<> hdrBuilder(clonedHeader);
      for (auto& I : *header) {
        auto* cloned = I.clone();
        hdrBuilder.Insert(cloned);
        VMap[&I] = cloned;
      }

      IRBuilder<> latBuilder(clonedLatch);
      for (auto& I : *latch) {
        auto* cloned = I.clone();
        latBuilder.Insert(cloned);
        VMap[&I] = cloned;
      }

      // ====== 统一重映射操作数 ======
      for (auto* clonedBB : { clonedHeader, clonedLatch }) {
        for (auto& I : *clonedBB) {
          // 跳过 PHI 的 block 参数（只重映射 value 参数）
          if (auto* phi = dyn_cast<PHINode>(&I)) {
            for (unsigned i = 0; i < phi->getNumIncomingValues(); ++i) {
              if (auto* mapped = VMap.lookup(phi->getIncomingValue(i)))
                phi->setIncomingValue(i, mapped);
              if (auto* mappedBB = VMap.lookup(phi->getIncomingBlock(i)))
                phi->setIncomingBlock(i, cast<BasicBlock>(mappedBB));
            }
          } else {
            for (unsigned i = 0; i < I.getNumOperands(); ++i) {
              if (auto* mapped = VMap.lookup(I.getOperand(i)))
                I.setOperand(i, mapped);
            }
          }
        }
      }

      // ====== 重连控制流 ======
      // 原始 latch → clonedHeader
      auto* latchTerm = latch->getTerminator();
      for (unsigned i = 0; i < latchTerm->getNumSuccessors(); ++i)
        if (latchTerm->getSuccessor(i) == header)
          latchTerm->setSuccessor(i, clonedHeader);

      // 克隆 latch → header（回边）
      auto* clonedLatchTerm = clonedLatch->getTerminator();
      for (unsigned i = 0; i < clonedLatchTerm->getNumSuccessors(); ++i)
        if (clonedLatchTerm->getSuccessor(i) == clonedHeader)
          clonedLatchTerm->setSuccessor(i, header);

      // 克隆 header：继续循环 → clonedLatch，退出 → header（回原始 header 重判）
      auto* clonedHeaderTerm = clonedHeader->getTerminator();
      for (unsigned i = 0; i < clonedHeaderTerm->getNumSuccessors(); ++i) {
        if (clonedHeaderTerm->getSuccessor(i) == latch)
          clonedHeaderTerm->setSuccessor(i, clonedLatch);
        else if (clonedHeaderTerm->getSuccessor(i) == exitBlock)
          clonedHeaderTerm->setSuccessor(i, header);
      }

      // ====== 修复 PHI ======
      // 新 CFG: preheader → header, clonedLatch → header (回边), clonedHeader → header (exit)
      //          latch → clonedHeader
      for (auto& origPhi : header->phis()) {
        auto* clonedPhi = cast<PHINode>(VMap[&origPhi]);

        int origLatchIdx = origPhi.getBasicBlockIndex(latch);
        int clonedPreheaderIdx = clonedPhi->getBasicBlockIndex(preheader);
        int clonedLatchIdx = clonedPhi->getBasicBlockIndex(clonedLatch);

        Value* origLatchVal = origLatchIdx >= 0
          ? origPhi.getIncomingValue(origLatchIdx)
          : nullptr;

        // 原始 header PHI：latch incoming → clonedLatch + cloned value
        if (origLatchIdx >= 0) {
          origPhi.setIncomingBlock(origLatchIdx, clonedLatch);
          origPhi.setIncomingValue(origLatchIdx, VMap[origLatchVal]);
        }

        // 原始 header PHI：添加来自 cloned header 的 incoming
        // （克隆 header 退出时回到原始 header 重判条件）
        origPhi.addIncoming(clonedPhi, clonedHeader);

        // 克隆 header PHI：preheader incoming → latch + 原始 latch value
        if (clonedPreheaderIdx >= 0 && origLatchVal) {
          clonedPhi->setIncomingBlock(clonedPreheaderIdx, latch);
          clonedPhi->setIncomingValue(clonedPreheaderIdx, origLatchVal);
        }

        // 移除 cloned header PHI 中 clonedLatch 的 incoming
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