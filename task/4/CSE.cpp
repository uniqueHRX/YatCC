#include "CSE.hpp"

using namespace llvm;

// 定义表达式结构体
struct Expression
{
  unsigned opcode;
  std::vector<Value*> operands;

  Expression(unsigned op, std::vector<Value*> ops)
  : opcode(op), operands(std::move(ops))
  {
  }
  // 重载表达式全等运算符
  bool operator==(const Expression& other) const
  {
    return opcode == other.opcode && operands == other.operands;
  }
  // 定义表达式的哈希函数
  friend hash_code hash_value(const Expression& expr)
  {
    // 使用LLVM的哈希组合函数来计算表达式的哈希值，结合操作码和操作数列表
    return hash_combine(expr.opcode,
      hash_combine_range(expr.operands.begin(), expr.operands.end()));
  }
};

// 为LLVM的DenseMap提供Expression类型的特化
template<>
struct llvm::DenseMapInfo<Expression>
{
  static inline Expression getEmptyKey()
  {
    return Expression(~0U, {});
  }
  static inline Expression getTombstoneKey()
  {
    return Expression(~0U - 1, {});
  }
  static unsigned getHashValue(const Expression& expr)
  {
    return hash_value(expr);
  }
  static bool isEqual(const Expression& lhs, const Expression& rhs)
  {
    return lhs == rhs;
  }
};

PreservedAnalyses
CSE::run(Module& mod, ModuleAnalysisManager& mam)
{
  int cseTimes = 0;

  // 遍历模块中的每个函数
  for (Function& func : mod) {
    if (func.isDeclaration()) continue;
    // 获取函数的支配树分析结果
    DominatorTree DT(func);
    auto* root = DT.getRoot();
    if (!root) continue;
    // 使用一个哈希表来存储已经计算过的表达式及其对应的值
    DenseMap<Expression, Instruction*> exprMap;
    // 遍历函数中的每个基本块
    for (BasicBlock& bb : func) {
      std::vector<Instruction*> toErase;
      // 遍历基本块中的每条指令
      for (Instruction& inst : bb) {
        // 处理二元运算指令
        if (auto binOp = dyn_cast<BinaryOperator>(&inst)) {
          // 解析指令参数
          auto opcode = binOp->getOpcode();
          std::vector<Value*> operands;
          for (unsigned i = 0; i < binOp->getNumOperands(); ++i) {
            operands.push_back(binOp->getOperand(i));
          }
          if (binOp->isCommutative()) {
            // 对于交换律的指令，排序操作数以规范化表达式
            std::sort(operands.begin(), operands.end());
          }
          Expression expr(opcode, operands);
          // 检查哈希表中是否已经存在相同的表达式
          auto it = exprMap.find(expr);
          if (it != exprMap.end()) {
            // 如果存在相同的表达式，说明当前指令是冗余的，可以被替换为之前计算过的值
            if (DT.dominates(it->second, &inst)) {
              // 如果之前计算过的值支配当前指令，说明当前指令可以被替换
              inst.replaceAllUsesWith(it->second);
              toErase.push_back(&inst);
              ++cseTimes;
            }
            else {
              // 如果之前计算过的值不支配当前指令，需要将当前表达式和对应的值存入哈希表
              exprMap[expr] = &inst;
            }
          } 
          else {
            // 如果不存在相同的表达式，将当前表达式和对应的值存入哈希表
            exprMap[expr] = &inst;
          }
        }
        // 处理比较指令
        else if (auto icmp = dyn_cast<ICmpInst>(&inst)) {
          auto opcode = icmp->getPredicate();
          std::vector<Value*> operands;
          for (unsigned i = 0; i < icmp->getNumOperands(); ++i) {
            operands.push_back(icmp->getOperand(i));
          }
          if (icmp->isCommutative()) {
            std::sort(operands.begin(), operands.end());
          }
          Expression expr(opcode, operands);
          auto it = exprMap.find(expr);
          if (it != exprMap.end()) {
            if (DT.dominates(it->second, &inst)) {
              inst.replaceAllUsesWith(it->second);
              toErase.push_back(&inst);
              ++cseTimes;
            }
            else {
              exprMap[expr] = &inst;
            }
          } 
          else {
            exprMap[expr] = &inst;
          }
        }
      }
      // 删除被替换掉的指令
      for (Instruction* inst : toErase) {
        inst->eraseFromParent();
      }
    }
  }
  mOut << "CSE running...\nTo eliminate " << cseTimes 
  << " instructions\n";
  return PreservedAnalyses::all();
}