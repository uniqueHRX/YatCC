#include "ConstantFolding.hpp"

using namespace llvm;

PreservedAnalyses
ConstantFolding::run(Module& mod, ModuleAnalysisManager& mam)
{
  int constFoldTimes = 0;

  // 遍历所有函数
  for (auto& func : mod) {
    // 遍历每个函数的基本块
    for (auto& bb : func) {
      std::vector<Instruction*> instToErase;
      // 遍历每个基本块的指令
      for (auto& inst : bb) {
        // 判断当前指令是否是二元运算指令
        if (auto binOp = dyn_cast<BinaryOperator>(&inst)) {
          // 获取二元运算指令的左右操作数，并尝试转换为常整数
          Value* lhs = binOp->getOperand(0);
          Value* rhs = binOp->getOperand(1);
          auto constLhs = dyn_cast<ConstantInt>(lhs);
          auto constRhs = dyn_cast<ConstantInt>(rhs);
          switch (binOp->getOpcode()) {
            case Instruction::Add: {
              // 若左右操作数均为整数常量，则进行常量折叠与use替换
              if (constLhs && constRhs) {
                binOp->replaceAllUsesWith(ConstantInt::getSigned(
                  binOp->getType(),
                  constLhs->getSExtValue() + constRhs->getSExtValue()));
                instToErase.push_back(binOp);
                ++constFoldTimes;
              }
              break;
            }
            case Instruction::Sub: {
              if (constLhs && constRhs) {
                binOp->replaceAllUsesWith(ConstantInt::getSigned(
                  binOp->getType(),
                  constLhs->getSExtValue() - constRhs->getSExtValue()));
                instToErase.push_back(binOp);
                ++constFoldTimes;
              }
              break;
            }
            case Instruction::Mul: {
              if (constLhs && constRhs) {
                binOp->replaceAllUsesWith(ConstantInt::getSigned(
                  binOp->getType(),
                  constLhs->getSExtValue() * constRhs->getSExtValue()));
                instToErase.push_back(binOp);
                ++constFoldTimes;
              }
              break;
            }
            case Instruction::UDiv:
            case Instruction::SDiv: {
              if (constLhs && constRhs) {
                binOp->replaceAllUsesWith(ConstantInt::getSigned(
                  binOp->getType(),
                  constLhs->getSExtValue() / constRhs->getSExtValue()));
                instToErase.push_back(binOp);
                ++constFoldTimes;
              }
              break;
            }
            case Instruction::URem:
            case Instruction::SRem: {
              if (constLhs && constRhs) {
                binOp->replaceAllUsesWith(ConstantInt::getSigned(
                  binOp->getType(),
                  constLhs->getSExtValue() % constRhs->getSExtValue()));
                instToErase.push_back(binOp);
                ++constFoldTimes;
              }
              break;
            }
            case Instruction::Shl: {
              if (constLhs && constRhs) {
                binOp->replaceAllUsesWith(ConstantInt::getSigned(
                  binOp->getType(),
                  constLhs->getSExtValue() << constRhs->getSExtValue()));
                instToErase.push_back(binOp);
                ++constFoldTimes;
              }
              break;
            }
            case Instruction::AShr: {
              if (constLhs && constRhs) {
                binOp->replaceAllUsesWith(ConstantInt::getSigned(
                  binOp->getType(),
                  constLhs->getSExtValue() >> constRhs->getSExtValue()));
                instToErase.push_back(binOp);
                ++constFoldTimes;
              }
              break;
            }
            case Instruction::LShr: {
              if (constLhs && constRhs) {
                binOp->replaceAllUsesWith(ConstantInt::getSigned(
                  binOp->getType(),
                  constLhs->getSExtValue() >> constRhs->getSExtValue()));
                instToErase.push_back(binOp);
                ++constFoldTimes;
              }
              break;
            }
            case Instruction::And: {
              if (constLhs && constRhs) {
                binOp->replaceAllUsesWith(ConstantInt::getSigned(
                  binOp->getType(),
                  constLhs->getSExtValue() & constRhs->getSExtValue()));
                instToErase.push_back(binOp);
                ++constFoldTimes;
              }
              break;
            }
            case Instruction::Or: {
              if (constLhs && constRhs) {
                binOp->replaceAllUsesWith(ConstantInt::getSigned(
                  binOp->getType(),
                  constLhs->getSExtValue() | constRhs->getSExtValue()));
                instToErase.push_back(binOp);
                ++constFoldTimes;
              }
              break;
            }
            case Instruction::Xor: {
              if (constLhs && constRhs) {
                binOp->replaceAllUsesWith(ConstantInt::getSigned(
                  binOp->getType(),
                  constLhs->getSExtValue() ^ constRhs->getSExtValue()));
                instToErase.push_back(binOp);
                ++constFoldTimes;
              }
              break;
            }
            default:
              break;
          }
        }
        // 判断当前指令是否是整数比较指令 (对应 ICmpSGT/SLT/SGE/SLE/EQ/NE)
        if (auto icmp = dyn_cast<ICmpInst>(&inst)) {
          Value* lhs = icmp->getOperand(0);
          Value* rhs = icmp->getOperand(1);
          auto constLhs = dyn_cast<ConstantInt>(lhs);
          auto constRhs = dyn_cast<ConstantInt>(rhs);
          if (constLhs && constRhs) {
            bool result;
            switch (icmp->getPredicate()) {
              case CmpInst::ICMP_SGT:
                result =
                  constLhs->getSExtValue() > constRhs->getSExtValue();
                break;
              case CmpInst::ICMP_SLT:
                result =
                  constLhs->getSExtValue() < constRhs->getSExtValue();
                break;
              case CmpInst::ICMP_SGE:
                result =
                  constLhs->getSExtValue() >= constRhs->getSExtValue();
                break;
              case CmpInst::ICMP_SLE:
                result =
                  constLhs->getSExtValue() <= constRhs->getSExtValue();
                break;
              case CmpInst::ICMP_EQ:
                result =
                  constLhs->getSExtValue() == constRhs->getSExtValue();
                break;
              case CmpInst::ICMP_NE:
                result =
                  constLhs->getSExtValue() != constRhs->getSExtValue();
                break;
              default:
                continue;
            }
            icmp->replaceAllUsesWith(
              ConstantInt::getBool(icmp->getContext(), result));
            instToErase.push_back(icmp);
            ++constFoldTimes;
          }
        }

      }
      // 统一删除被折叠为常量的指令
      for (auto& i : instToErase)
        i->eraseFromParent();
    }
  }

  mOut << "ConstantFolding running...\nTo eliminate " << constFoldTimes
       << " instructions\n";
  return PreservedAnalyses::all();
}
