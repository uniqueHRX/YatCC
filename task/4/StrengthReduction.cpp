#include "StrengthReduction.hpp"

using namespace llvm;

PreservedAnalyses
StrengthReduction::run(Module& mod, ModuleAnalysisManager& mam)
{
  int strengthReduceTimes = 0;

  // 遍历所有函数
  for (auto& func : mod) {
    // 遍历每个函数的基本块
    for (auto& bb : func) {
      std::vector<Instruction*> instToErase;
      // 遍历每个基本块的指令
      for (auto& inst : bb) {
        // 判断当前指令是否是二元运算指令
        if (auto binOp = dyn_cast<BinaryOperator>(&inst)) {
          // 尝试转换二元运算指令的左右操作数为常整数
          Value* lhs = binOp->getOperand(0);
          Value* rhs = binOp->getOperand(1);
          auto constLhs = dyn_cast<ConstantInt>(lhs);
          auto constRhs = dyn_cast<ConstantInt>(rhs);
          switch (binOp->getOpcode()) {
            case Instruction::Mul: {
              // 如果乘法指令的一个操作数是常整数且该常整数是2的幂次方，则进行强度削弱优化
              if (constLhs && constLhs->getValue().isPowerOf2()) {
                unsigned shiftAmount = llvm::Log2_64(constLhs->getZExtValue());
                IRBuilder<> builder(&inst);
                Value* shiftInst = builder.CreateShl(rhs, shiftAmount);
                inst.replaceAllUsesWith(shiftInst);
                instToErase.push_back(&inst);
                ++strengthReduceTimes;
              } 
              else if (constRhs && constRhs->getValue().isPowerOf2()) {
                unsigned shiftAmount = llvm::Log2_64(constRhs->getZExtValue());
                IRBuilder<> builder(&inst);
                Value* shiftInst = builder.CreateShl(lhs, shiftAmount);
                inst.replaceAllUsesWith(shiftInst);
                instToErase.push_back(&inst);
                ++strengthReduceTimes;
              }
              break;
            }
            case Instruction::UDiv: {
              // 如果除法指令的除数是常整数且该常整数是2的幂次方，则进行强度削弱优化
              if (constRhs && constRhs->getValue().isPowerOf2()) {
                unsigned shiftAmount = llvm::Log2_64(constRhs->getZExtValue());
                IRBuilder<> builder(&inst);
                Value* shiftInst = builder.CreateLShr(lhs, shiftAmount);
                inst.replaceAllUsesWith(shiftInst);
                instToErase.push_back(&inst);
                ++strengthReduceTimes;
              }
              break;
            }
            case Instruction::SDiv: {
              // 有符号除法 x / 2^k  →  (x + bias) >> k
              if (constRhs && constRhs->getValue().isPowerOf2()) {
                unsigned bitWidth = binOp->getType()->getIntegerBitWidth();
                unsigned shiftAmount = llvm::Log2_64(constRhs->getZExtValue());
                IRBuilder<> builder(&inst);
                // signMask = ashr x, N-1   → 全1（负数）或全0（非负）
                Value* signMask = builder.CreateAShr(lhs, bitWidth - 1);
                // bias = lshr signMask, N-k  → 负数时 = 2^k-1, 非负时 = 0
                Value* bias = builder.CreateLShr(signMask, bitWidth - shiftAmount);
                Value* biased = builder.CreateAdd(lhs, bias);
                Value* shiftInst = builder.CreateAShr(biased, shiftAmount);
                inst.replaceAllUsesWith(shiftInst);
                instToErase.push_back(&inst);
                ++strengthReduceTimes;
              }
              break;
            }
            case Instruction::URem: {
              // 如果取模指令的模数是常整数且该常整数是2的幂次方，则进行强度削弱优化
              if (constRhs && constRhs->getValue().isPowerOf2()) {
                IRBuilder<> builder(&inst);
                Value* andInst = builder.CreateAnd(lhs, constRhs->getValue() - 1);
                inst.replaceAllUsesWith(andInst);
                instToErase.push_back(&inst);
                ++strengthReduceTimes;
              }
              break;
            }
            case Instruction::SRem: {
              // 有符号取模 x % 2^k  →  x - ((x + bias) >> k) << k
              if (constRhs && constRhs->getValue().isPowerOf2()) {
                unsigned bitWidth = binOp->getType()->getIntegerBitWidth();
                unsigned shiftAmount = llvm::Log2_64(constRhs->getZExtValue());
                IRBuilder<> builder(&inst);
                Value* signMask = builder.CreateAShr(lhs, bitWidth - 1);
                Value* bias = builder.CreateLShr(signMask, bitWidth - shiftAmount);
                Value* biased = builder.CreateAdd(lhs, bias);
                Value* quotient = builder.CreateAShr(biased, shiftAmount);
                Value* prod = builder.CreateShl(quotient, shiftAmount);
                Value* remInst = builder.CreateSub(lhs, prod);
                inst.replaceAllUsesWith(remInst);
                instToErase.push_back(&inst);
                ++strengthReduceTimes;
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

  mOut << "StrengthReduction running...\nTo reduce " << strengthReduceTimes 
  << " instructions\n";
  return PreservedAnalyses::all();
}