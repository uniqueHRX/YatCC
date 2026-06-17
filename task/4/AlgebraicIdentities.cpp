#include "AlgebraicIdentities.hpp"

using namespace llvm;

PreservedAnalyses
AlgebraicIdentities::run(Module& mod, ModuleAnalysisManager& mam)
{
  int algebraicIdentifyTimes = 0;

  // 遍历所有函数
  for (auto& func : mod) {
    // 遍历每个函数的基本块
    for (auto& bb : func) {
      std::vector<Instruction*> instToErase;
      // 遍历每个基本块的指令
      for (auto& inst : bb) {
        // 判断当前指令是否是二元运算指令
        if (auto binOp = dyn_cast<BinaryOperator>(&inst)) {
          Value* lhs = binOp->getOperand(0);
          Value* rhs = binOp->getOperand(1);
          auto constLhs = dyn_cast<ConstantInt>(lhs);
          auto constRhs = dyn_cast<ConstantInt>(rhs);

          switch (binOp->getOpcode()) {
            case Instruction::Add: {
              // x + 0 → x
              if (constRhs && constRhs->isZero()) {
                inst.replaceAllUsesWith(lhs);
                instToErase.push_back(&inst);
                ++algebraicIdentifyTimes;
              }
              // 0 + x → x
              else if (constLhs && constLhs->isZero()) {
                inst.replaceAllUsesWith(rhs);
                instToErase.push_back(&inst);
                ++algebraicIdentifyTimes;
              }
              break;
            }
            case Instruction::Sub: {
              // x - 0 → x
              if (constRhs && constRhs->isZero()) {
                inst.replaceAllUsesWith(lhs);
                instToErase.push_back(&inst);
                ++algebraicIdentifyTimes;
              }
              // 0 - x → -x
              else if (constLhs && constLhs->isZero()) {
                IRBuilder<> builder(&inst);
                Value* negInst = builder.CreateNeg(rhs);
                inst.replaceAllUsesWith(negInst);
                instToErase.push_back(&inst);
                ++algebraicIdentifyTimes;
              }
              // x - x → 0
              else if (lhs == rhs) {
                auto zero = ConstantInt::get(binOp->getType(), 0);
                inst.replaceAllUsesWith(zero);
                instToErase.push_back(&inst);
                ++algebraicIdentifyTimes;
              }
              break;
            }
            case Instruction::Mul: {
              // x * 1 → x
              if (constRhs && constRhs->isOne()) {
                inst.replaceAllUsesWith(lhs);
                instToErase.push_back(&inst);
                ++algebraicIdentifyTimes;
              }
              // x * 0 → 0
              else if (constRhs && constRhs->isZero()) {
                inst.replaceAllUsesWith(constRhs);
                instToErase.push_back(&inst);
                ++algebraicIdentifyTimes;
              }
              // x * -1 → -x
              else if (constRhs && constRhs->isMinusOne()) {
                IRBuilder<> builder(&inst);
                Value* negInst = builder.CreateNeg(lhs);
                inst.replaceAllUsesWith(negInst);
                instToErase.push_back(&inst);
                ++algebraicIdentifyTimes;
              }
              // 1 * x → x
              else if (constLhs && constLhs->isOne()) {
                inst.replaceAllUsesWith(rhs);
                instToErase.push_back(&inst);
                ++algebraicIdentifyTimes;
              }
              // 0 * x → 0
              else if (constLhs && constLhs->isZero()) {
                inst.replaceAllUsesWith(constLhs);
                instToErase.push_back(&inst);
                ++algebraicIdentifyTimes;
              }
              // -1 * x → -x
              else if (constLhs && constLhs->isMinusOne()) {
                IRBuilder<> builder(&inst);
                Value* negInst = builder.CreateNeg(rhs);
                inst.replaceAllUsesWith(negInst);
                instToErase.push_back(&inst);
                ++algebraicIdentifyTimes;
              }
              break;
            }
            case Instruction::UDiv:
            case Instruction::SDiv: {
              // x / 1 → x
              if (constRhs && constRhs->isOne()) {
                inst.replaceAllUsesWith(lhs);
                instToErase.push_back(&inst);
                ++algebraicIdentifyTimes;
              }
              // 0 / x → 0
              else if (constLhs && constLhs->isZero()) {
                inst.replaceAllUsesWith(constLhs);
                instToErase.push_back(&inst);
                ++algebraicIdentifyTimes;
              }
              break;
            }
            case Instruction::URem:
            case Instruction::SRem: {
              // x % 1 → 0
              if (constRhs && constRhs->isOne()) {
                auto zero = ConstantInt::get(binOp->getType(), 0);
                inst.replaceAllUsesWith(zero);
                instToErase.push_back(&inst);
                ++algebraicIdentifyTimes;
              }
              // 0 % x → 0
              else if (constLhs && constLhs->isZero()) {
                auto zero = ConstantInt::get(binOp->getType(), 0);
                inst.replaceAllUsesWith(zero);
                instToErase.push_back(&inst);
                ++algebraicIdentifyTimes;
              }
              break;
            }
            case Instruction::Shl: {
              // x << 0 → x
              if (constRhs && constRhs->isZero()) {
                inst.replaceAllUsesWith(lhs);
                instToErase.push_back(&inst);
                ++algebraicIdentifyTimes;
              }
              break;
            }
            case Instruction::AShr:
            case Instruction::LShr: {
              // x >> 0 → x
              if (constRhs && constRhs->isZero()) {
                inst.replaceAllUsesWith(lhs);
                instToErase.push_back(&inst);
                ++algebraicIdentifyTimes;
              }
              break;
            }
            case Instruction::And: {
              // x & 0 → 0
              if (constRhs && constRhs->isZero()) {
                inst.replaceAllUsesWith(constRhs);
                instToErase.push_back(&inst);
                ++algebraicIdentifyTimes;
              }
              // x & -1 → x
              else if (constRhs && constRhs->isMinusOne()) {
                inst.replaceAllUsesWith(lhs);
                instToErase.push_back(&inst);
                ++algebraicIdentifyTimes;
              }
              // 0 & x → 0
              else if (constLhs && constLhs->isZero()) {
                inst.replaceAllUsesWith(constLhs);
                instToErase.push_back(&inst);
                ++algebraicIdentifyTimes;
              }
              // -1 & x → x
              else if (constLhs && constLhs->isMinusOne()) {
                inst.replaceAllUsesWith(rhs);
                instToErase.push_back(&inst);
                ++algebraicIdentifyTimes;
              }
              // x & x → x
              else if (lhs == rhs) {
                inst.replaceAllUsesWith(lhs);
                instToErase.push_back(&inst);
                ++algebraicIdentifyTimes;
              }
              break;
            }
            case Instruction::Or: {
              // x | 0 → x
              if (constRhs && constRhs->isZero()) {
                inst.replaceAllUsesWith(lhs);
                instToErase.push_back(&inst);
                ++algebraicIdentifyTimes;
              }
              // x | -1 → -1
              else if (constRhs && constRhs->isMinusOne()) {
                inst.replaceAllUsesWith(constRhs);
                instToErase.push_back(&inst);
                ++algebraicIdentifyTimes;
              }
              // 0 | x → x
              else if (constLhs && constLhs->isZero()) {
                inst.replaceAllUsesWith(rhs);
                instToErase.push_back(&inst);
                ++algebraicIdentifyTimes;
              }
              // -1 | x → -1
              else if (constLhs && constLhs->isMinusOne()) {
                inst.replaceAllUsesWith(constLhs);
                instToErase.push_back(&inst);
                ++algebraicIdentifyTimes;
              }
              // x | x → x
              else if (lhs == rhs) {
                inst.replaceAllUsesWith(lhs);
                instToErase.push_back(&inst);
                ++algebraicIdentifyTimes;
              }
              break;
            }
            case Instruction::Xor: {
              // x ^ 0 → x
              if (constRhs && constRhs->isZero()) {
                inst.replaceAllUsesWith(lhs);
                instToErase.push_back(&inst);
                ++algebraicIdentifyTimes;
              }
              // 0 ^ x → x
              else if (constLhs && constLhs->isZero()) {
                inst.replaceAllUsesWith(rhs);
                instToErase.push_back(&inst);
                ++algebraicIdentifyTimes;
              }
              // x ^ x → 0
              else if (lhs == rhs) {
                auto zero = ConstantInt::get(binOp->getType(), 0);
                inst.replaceAllUsesWith(zero);
                instToErase.push_back(&inst);
                ++algebraicIdentifyTimes;
              }
              break;
            }
            default:
              break;
          }
        }
      }
      // 删除被替换掉的指令
      for (auto* inst : instToErase) {
        inst->eraseFromParent();
      }
    }
  }

  mOut << "AlgebraicIdentities running...\nTo identify " << algebraicIdentifyTimes
       << " algebraic identities\n";
  return PreservedAnalyses::all();
}