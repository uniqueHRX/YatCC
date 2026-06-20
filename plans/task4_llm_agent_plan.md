# 基于LLM Agent的编译优化Pass序列自动搜索 — 增强方案

## 1. 现状分析

### 1.1 现有架构

当前 [`PassSequencePredict`](../task/4/llm/PassSequencePredict.cpp) 工作流程：

```
Source IR (O0) → LLM分析 → 预测Pass序列 → 执行Pass → 输出IR
```

核心流程：
1. **Pass摘要生成**：LLM阅读每个Pass的C++源码，生成功能描述（缓存）
2. **IR分析**：将完整LLVM IR文本 + Pass摘要发送给LLM
3. **序列预测**：LLM返回一个逗号分隔的Pass名称序列
4. **执行**：按序执行Pass，输出优化后的IR

### 1.2 现有问题

| 问题 | 描述 | 影响 |
|------|------|------|
| 单次推理 | 只调用一次LLM，无反馈迭代 | 无法修正错误决策 |
| 无代价评估 | 不执行Pass也不评估IR变化 | 无法判断序列优劣 |
| 仅单候选 | 只生成一个序列 | 缺乏选择空间 |
| 原始IR过大 | 发送完整IR文本（mm.ll约350行） | token消耗大，LLM注意力分散 |
| 无IR特征 | 不给LLM提供结构化特征 | LLM需要自行分析大段IR |
| 无过程记录 | 不记录中间结果 | 无法做ablation study |

### 1.3 当前得分

`mm.sysu.c`: 74.33/100（O2用时1459563us，Agent用时2642071us）

## 2. 增强方案总体架构

### 2.1 核心改进：迭代反馈Agent

```
┌─────────────────────────────────────────────────────┐
│              IR Feature Extractor                     │
│  ┌──────────┐  ┌──────────┐  ┌──────────────────┐   │
│  │ 基本块数  │  │ 指令数   │  │ 循环嵌套深度      │   │
│  │ 函数调用数│  │ 内存操作数│  │ 分支比例          │   │
│  │ 向量化潜力│  │ 常量占比 │  │ 别名分析复杂度    │   │
│  └──────────┘  └──────────┘  └──────────────────┘   │
└──────────────────────┬──────────────────────────────┘
                       │
                       ▼
┌─────────────────────────────────────────────────────┐
│                  LLM Agent Loop                       │
│                                                       │
│  ┌────────────┐    ┌──────────────┐                  │
│  │ IR特征     │───▶│ Prompt组装   │                  │
│  │ 历史记录   │    │ (含历史反馈) │                  │
│  └────────────┘    └──────┬───────┘                  │
│                           │                           │
│                           ▼                           │
│                    ┌──────────────┐                   │
│                    │  LLM推理     │                   │
│                    │  生成K个候选 │                   │
│                    └──────┬───────┘                   │
│                           │                           │
│                           ▼                           │
│                    ┌──────────────┐                   │
│                    │  Pass执行    │                   │
│                    │  与代价评估  │                   │
│                    └──────┬───────┘                   │
│                           │                           │
│                    ┌──────▼───────┐                   │
│                    │ 快速评估     │                   │
│                    │ (IR指令数)   │                   │
│                    └──────┬───────┘                   │
│                           │                           │
│                           ▼                           │
│                    ┌──────────────┐                   │
│              ┌─────│ 收敛?        │─────┐             │
│              │ 否  └──────────────┘  是 │             │
│              ▼                          ▼             │
│     ┌──────────────┐           ┌──────────────┐      │
│     │ 反馈：IR变化 │           │ 输出最优IR   │      │
│     │ 下次迭代建议 │           │              │      │
│     └──────────────┘           └──────────────┘      │
└─────────────────────────────────────────────────────┘
```

### 2.2 关键组件

#### A. IR Feature Extractor

在C++端实现，从LLVM Module中提取结构化特征：

```cpp
struct IRFeatures {
  // 基本统计
  unsigned numFunctions;
  unsigned numBasicBlocks;
  unsigned numInstructions;
  unsigned numMemoryOps;  // load/store
  unsigned numCalls;
  unsigned numBranches;
  
  // 循环特征
  unsigned numLoops;
  unsigned maxLoopDepth;
  double avgLoopTripCount;  // 需要SCEV
  
  // 指令分布
  unsigned numArithOps;   // add/sub/mul/div
  unsigned numPhiNodes;
  unsigned numAllocas;
  unsigned numGEPs;
  unsigned numCmpOps;
  
  // 优化潜力指标
  double constantRatio;      // 常量操作数占比
  double memOpRatio;         // 内存操作占比
  unsigned numTrivialBB;     // 单指令基本块
  bool hasNestedLoops;
  bool hasFunctionCalls;     // 是否有函数调用
  unsigned numUnusedValues;  // 潜在死代码
};
```

#### B. IR Cost Estimator

快速评估IR质量（不编译运行）：

- **指令数**：最直接的体积/性能指标
- **内存操作数**：load/store越多性能越差
- **基本块数**：反映控制流复杂度
- **组合评分**：`cost = 0.5 * instrCount + 0.3 * memOpCount + 0.2 * bbCount`

#### C. 迭代Agent循环

```
Round 1: IR特征 + Prompt → LLM → 3个候选序列 → 执行 → 评估 → 选最优
Round 2: 最优IR + 变化分析 + 改进建议 → LLM → 3个候选序列 → 执行 → 评估
Round 3: ... (直到收敛或达到最大轮次)
```

收敛条件：
- 连续2轮IR指令数变化 < 1%
- 或达到最大轮次（默认3轮）

#### D. 多候选生成

每轮LLM生成3个候选序列，快速评估后选最优：

```
LLM响应格式:
<response>
  <candidates>
    <candidate id="1">
      <reasoning>Mem2Reg消除alloca+store/load，为后续优化做准备...</reasoning>
      <sequence>Mem2Reg,ConstantPropagation,ConstantFolding,CSE,DCE,LICM,LoopUnrolling,InstructionCombining,DSE</sequence>
    </candidate>
    <candidate id="2">
      ...
    </candidate>
    <candidate id="3">
      ...
    </candidate>
  </candidates>
</response>
```

#### E. 增强Prompt设计

System Prompt 新增内容：
- IR特征解释指南（告诉LLM如何看待循环深度、内存操作比等）
- 迭代历史格式（包含上一轮的IR变化）
- 多候选生成要求
- Pass间依赖关系知识（如Mem2Reg应在其他Pass之前）
- 已知有效序列模式（如 const-prop→const-fold→dce→cse）

## 3. 实现文件结构

```
task/4/llm/
├── __init__.py                    # (已有) LLMHelperImpl
├── LLMHelper.hpp/cpp              # (已有) LLM会话管理
├── PassSequencePredict.hpp/cpp    # (改造) 增强的Agent主逻辑
├── IRFeatureExtractor.hpp/cpp     # (新增) IR特征提取
├── IRCostEstimator.hpp/cpp        # (新增) IR代价评估
├── prompts/
│   ├── PassSummarySysPrTpl.xml    # (已有) Pass摘要系统提示词
│   ├── PassSummaryUserPrTpl.xml   # (已有) Pass摘要用户提示词
│   ├── PassSeqPredSysPrTpl.xml    # (改造) 增强的系统提示词
│   └── PassSeqPredUserPrTpl.xml   # (改造) 增强的用户提示词
```

## 4. 测试集设计

### 4.1 现有测例

| 测例 | 特点 | 用途 |
|------|------|------|
| `mm.sysu.c` | 矩阵乘法，三重循环 | 基础性能测例 |
| `fft.sysu.c` | 快速傅里叶变换，递归 | 基础性能测例 |

### 4.2 新增测例

| 测例 | 特点 | Pass顺序敏感点 | 默认顺序的缺陷 |
|------|------|----------------|----------------|
| `chained-opt.sysu.c` | 多重循环+重复计算+代数恒等式 | CP/CF 在 LICM 之前；Unroll 后需再次 CSE | LICM 无法识别循环不变量；展开后冗余残留 |
| `inline-prop.sysu.c` | 多函数内联+常量传播+冗余写 | Inliner→Mem2Reg→CP/CF→LICM→CSE→DSE | LICM 在 CP/CF 之前；DSE 不在最后 |
| `loop-reduce.sysu.c` | 强度削减+展开+多轮CSE | StrengthReduction 前置；多轮 CSE/DCE | StrengthReduction 在最后；单轮 CSE 不够 |

## 5. 评价指标

### 5.1 主要指标

| 指标 | 计算方式 | 意义 |
|------|----------|------|
| 执行时间比 | `sqrt(O2_time / agent_time) * 100` | 现有评分标准 |
| IR指令数减少率 | `(input_instructions - output_instructions) / input_instructions` | 静态优化效果 |
| IR内存操作减少率 | 同上 | 内存优化效果 |

### 5.2 消融实验指标

| 实验组 | 条件 | 对比目标 |
|--------|------|----------|
| Baseline | 固定Pass序列（原始方案） | 验证LLM预测比固定序列好 |
| 单轮 vs 多轮 | 迭代次数=1/2/3 | 验证迭代反馈的价值 |
| 单候选 vs 多候选 | 候选数=1/3 | 验证多候选的价值 |
| 无特征 vs 有特征 | 是否提供IR特征 | 验证特征提取的价值 |

## 6. 实验流程

```
1. 准备阶段
   ├── 编写新增测试用例
   ├── 用clang -O2生成标准答案(answer.ll + answer.exe)
   └── 运行answer.exe获取baseline执行时间

2. 运行实验
   ├── 对每个测例，运行Agent（含多轮迭代）
   ├── 记录每轮的IR特征变化
   ├── 编译output.ll为output.exe并运行
   └── 记录执行时间

3. 消融实验
   ├── 关闭特征提取，重复实验
   ├── 限制迭代轮数=1，重复实验
   └── 关闭多候选，重复实验

4. 结果分析
   ├── 计算所有指标
   ├── 绘制对比图表
   └── 撰写实验报告
```

## 7. 实验报告大纲

1. **引言**：编译优化Pass序列搜索问题
2. **相关工作**：LLM辅助编译优化
3. **方法**：Agent架构、IR特征提取、迭代反馈、多候选
4. **实验设置**：测试集、实验环境、对比方法
5. **实验结果**：主实验、消融实验、案例分析
6. **讨论**：局限性、改进方向
7. **结论**
8. **附录**：复现指南（完整命令）

## 8. 实施步骤

| 序号 | 步骤 | 依赖 |
|------|------|------|
| 1 | 实现 IRFeatureExtractor (C++) | 无 |
| 2 | 实现 IRCostEstimator (C++) | 1 |
| 3 | 重新设计 Agent 主循环 (改造 PassSequencePredict) | 2 |
| 4 | 更新 Prompt 模板 | 3 |
| 5 | 编写新增测试用例 | 无 |
| 6 | 集成测试与调试 | 4, 5 |
| 7 | 运行消融实验 | 6 |
| 8 | 撰写实验报告 | 7 |