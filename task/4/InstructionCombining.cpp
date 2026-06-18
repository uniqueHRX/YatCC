#include "InstructionCombining.hpp"

using namespace llvm;

PreservedAnalyses
InstructionCombining::run(Module& mod, ModuleAnalysisManager& mam)
{
  int combineTimes = 0;
  
  // 遍历模块中的每个函数
  for (Function& func : mod) {
    // 遍历函数中的每个基本块
    for (BasicBlock& bb : func) {
      std::vector<Instruction*> toErase;
      // 遍历基本块中的每条指令
      for (Instruction& inst : bb) {
        // 处理二元运算指令
        if (auto binOp = dyn_cast<BinaryOperator>(&inst)) {
          auto opcode = binOp->getOpcode();
          Value* lhs = binOp->getOperand(0);
          Value* rhs = binOp->getOperand(1);
          // 判断操作数是否含有常数
          if (!isa<Constant>(lhs) && !isa<Constant>(rhs)) continue;
          // 处理符合交换律的指令
          if (binOp->isCommutative()) {
            // 如果左操作数是常数，交换左右操作数
            if (isa<Constant>(lhs) && !isa<Constant>(rhs)) {
              Value* temp = lhs;
              lhs = rhs;
              rhs = temp;
            }
          }
          // 分情况处理不同指令
          switch (opcode) {
            // 处理加法指令的常数合并
            case Instruction::Add: {
              // 提取当前指令的常量值
              APInt accumulated = cast<ConstantInt>(rhs)->getValue();
              Value* root = lhs;
              // 沿着链向上遍历并累加常量
              while (auto prevBinOp = dyn_cast<BinaryOperator>(root)) {
                // 只处理加法指令，并且要求前一个指令只有一个使用者
                if (prevBinOp->getOpcode() != Instruction::Add) break;
                if (!prevBinOp->hasOneUse()) break;
                Value* prevLhs = prevBinOp->getOperand(0);
                Value* prevRhs = prevBinOp->getOperand(1);
                // 判断是否有常量并确保常量在右侧
                if (!isa<Constant>(prevLhs) && !isa<Constant>(prevRhs)) break;
                if (isa<Constant>(prevLhs) && !isa<Constant>(prevRhs)) {
                  Value* temp = prevLhs;
                  prevLhs = prevRhs;
                  prevRhs = temp;
                }
                // 累加常量值并标记待删除指令
                accumulated += cast<ConstantInt>(prevRhs)->getValue();
                root = prevLhs;
                toErase.push_back(prevBinOp);
                ++combineTimes;
              }
              // 更新当前指令的操作数到链的根部
              binOp->setOperand(0, root);
              binOp->setOperand(1, ConstantInt::get(binOp->getContext(), accumulated));
              break;
            }
            // 处理减法指令的常数合并
            case Instruction::Sub: {
              // 减法不可交换，右操作数必须是常数
              if (!isa<ConstantInt>(rhs)) break;
              APInt accumulated = cast<ConstantInt>(rhs)->getValue();
              Value* root = lhs;
              // 只沿左操作数链向上追溯
              while (auto prevBinOp = dyn_cast<BinaryOperator>(root)) {
                if (prevBinOp->getOpcode() != Instruction::Sub) break;
                if (!prevBinOp->hasOneUse()) break;
                Value* prevLhs = prevBinOp->getOperand(0);
                Value* prevRhs = prevBinOp->getOperand(1);
                // 上一条 sub 的右操作数也必须是常数
                if (!isa<ConstantInt>(prevRhs)) break;
                // 累加常量值并标记待删除指令
                accumulated += cast<ConstantInt>(prevRhs)->getValue();
                root = prevLhs;
                toErase.push_back(prevBinOp);
                ++combineTimes;
              }
              binOp->setOperand(0, root);
              binOp->setOperand(1, ConstantInt::get(binOp->getContext(), accumulated));
              break;
            }
            // 处理乘法指令的常数合并
            case Instruction::Mul: {
              // 提取当前指令的常量值
              APInt accumulated = cast<ConstantInt>(rhs)->getValue();
              Value* root = lhs;
              // 沿着链向上遍历并累乘常量
              while (auto prevBinOp = dyn_cast<BinaryOperator>(root)) {
                // 只处理乘法指令，并且要求前一个指令只有一个使用者
                if (prevBinOp->getOpcode() != Instruction::Mul) break;
                if (!prevBinOp->hasOneUse()) break;
                Value* prevLhs = prevBinOp->getOperand(0);
                Value* prevRhs = prevBinOp->getOperand(1);
                // 判断是否有常量并确保常量在右侧
                if (!isa<Constant>(prevLhs) && !isa<Constant>(prevRhs)) break;
                if (isa<Constant>(prevLhs) && !isa<Constant>(prevRhs)) {
                  Value* temp = prevLhs;
                  prevLhs = prevRhs;
                  prevRhs = temp;
                }
                // 累乘常量值并标记待删除指令
                accumulated *= cast<ConstantInt>(prevRhs)->getValue();
                root = prevLhs;
                toErase.push_back(prevBinOp);
                ++combineTimes;
              }
              // 更新当前指令的操作数到链的根部
              binOp->setOperand(0, root);
              binOp->setOperand(1, ConstantInt::get(binOp->getContext(), accumulated));
              break;
            }
            // 处理有符号除法指令的常数合并
            case Instruction::SDiv: {
              // 除法不可交换，右操作数必须是常数
              if (!isa<ConstantInt>(rhs)) break;
              APInt accumulated = cast<ConstantInt>(rhs)->getValue();
              Value* root = lhs;
              while (auto prevBinOp = dyn_cast<BinaryOperator>(root)) {
                if (prevBinOp->getOpcode() != Instruction::SDiv) break;
                if (!prevBinOp->hasOneUse()) break;
                Value* prevLhs = prevBinOp->getOperand(0);
                Value* prevRhs = prevBinOp->getOperand(1);
                // 上一条 sdiv 的右操作数也必须是常数
                if (!isa<ConstantInt>(prevRhs)) break;
                // 累乘常量值并标记待删除指令
                accumulated *= cast<ConstantInt>(prevRhs)->getValue();
                root = prevLhs;
                toErase.push_back(prevBinOp);
                ++combineTimes;
              }
              binOp->setOperand(0, root);
              binOp->setOperand(1, ConstantInt::get(binOp->getContext(), accumulated));
              break;
            }
            // 处理无符号除法指令的常数合并
            case Instruction::UDiv: {
              if (!isa<ConstantInt>(rhs)) break;
              APInt accumulated = cast<ConstantInt>(rhs)->getValue();
              Value* root = lhs;
              while (auto prevBinOp = dyn_cast<BinaryOperator>(root)) {
                if (prevBinOp->getOpcode() != Instruction::UDiv) break;
                if (!prevBinOp->hasOneUse()) break;
                Value* prevLhs = prevBinOp->getOperand(0);
                Value* prevRhs = prevBinOp->getOperand(1);
                // 上一条 udiv 的右操作数也必须是常数
                if (!isa<ConstantInt>(prevRhs)) break;
                // 累乘常量值并标记待删除指令
                accumulated *= cast<ConstantInt>(prevRhs)->getValue();
                root = prevLhs;
                toErase.push_back(prevBinOp);
                ++combineTimes;
              }
              binOp->setOperand(0, root);
              binOp->setOperand(1, ConstantInt::get(binOp->getContext(), accumulated));
              break;
            }
            // 处理左移指令的常数合并
            case Instruction::Shl: {
              if (!isa<ConstantInt>(rhs)) break;
              APInt accumulated = cast<ConstantInt>(rhs)->getValue();
              Value* root = lhs;
              while (auto prevBinOp = dyn_cast<BinaryOperator>(root)) {
                if (prevBinOp->getOpcode() != Instruction::Shl) break;
                if (!prevBinOp->hasOneUse()) break;
                if (!isa<ConstantInt>(prevBinOp->getOperand(1))) break;
                accumulated += cast<ConstantInt>(prevBinOp->getOperand(1))->getValue();
                root = prevBinOp->getOperand(0);
                toErase.push_back(prevBinOp);
                ++combineTimes;
              }
              binOp->setOperand(0, root);
              binOp->setOperand(1, ConstantInt::get(binOp->getContext(), accumulated));
              break;
            }
            // 处理逻辑右移指令的常数合并
            case Instruction::LShr: {
              if (!isa<ConstantInt>(rhs)) break;
              APInt accumulated = cast<ConstantInt>(rhs)->getValue();
              Value* root = lhs;
              while (auto prevBinOp = dyn_cast<BinaryOperator>(root)) {
                if (prevBinOp->getOpcode() != Instruction::LShr) break;
                if (!prevBinOp->hasOneUse()) break;
                if (!isa<ConstantInt>(prevBinOp->getOperand(1))) break;
                accumulated += cast<ConstantInt>(prevBinOp->getOperand(1))->getValue();
                root = prevBinOp->getOperand(0);
                toErase.push_back(prevBinOp);
                ++combineTimes;
              }
              binOp->setOperand(0, root);
              binOp->setOperand(1, ConstantInt::get(binOp->getContext(), accumulated));
              break;
            }
            // 处理算术右移指令的常数合并
            case Instruction::AShr: {
              if (!isa<ConstantInt>(rhs)) break;
              APInt accumulated = cast<ConstantInt>(rhs)->getValue();
              Value* root = lhs;
              while (auto prevBinOp = dyn_cast<BinaryOperator>(root)) {
                if (prevBinOp->getOpcode() != Instruction::AShr) break;
                if (!prevBinOp->hasOneUse()) break;
                if (!isa<ConstantInt>(prevBinOp->getOperand(1))) break;
                accumulated += cast<ConstantInt>(prevBinOp->getOperand(1))->getValue();
                root = prevBinOp->getOperand(0);
                toErase.push_back(prevBinOp);
                ++combineTimes;
              }
              binOp->setOperand(0, root);
              binOp->setOperand(1, ConstantInt::get(binOp->getContext(), accumulated));
              break;
            }
            
            default:
              break; 
          }
        }
      }
      // 删除被合并的冗余指令
      for (Instruction* inst : toErase) {
        inst->eraseFromParent();
      }
    }
  }
  mOut << "InstructionCombining running...\nTo combine " << combineTimes 
  << " instructions\n";
  return PreservedAnalyses::all();
}