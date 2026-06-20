#include <iostream>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Verifier.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Support/raw_ostream.h>

#include "ConstantFolding.hpp"
#include "ConstantPropagation.hpp"
#include "StrengthReduction.hpp"
#include "AlgebraicIdentities.hpp"
#include "CSE.hpp"
#include "DCE.hpp"
#include "DSE.hpp"
#include "Inliner.hpp"
#include "InstructionCombining.hpp"
#include "LICM.hpp"
#include "LoopUnrolling.hpp"
#include "Mem2Reg.hpp"
#include "StaticCallCounter.hpp"
#include "StaticCallCounterPrinter.hpp"

#ifdef TASK4_LLM

#include <pybind11/embed.h>

#include "PassSequencePredict.hpp"

namespace Py = pybind11;

#endif

void
opt(llvm::Module& mod)
{
  using namespace llvm;

  // 定义分析pass的管理器
  LoopAnalysisManager lam;
  FunctionAnalysisManager fam;
  CGSCCAnalysisManager cgam;
  ModuleAnalysisManager mam;
  ModulePassManager mpm;

  // 注册分析pass的管理器
  PassBuilder pb;
  pb.registerModuleAnalyses(mam);
  pb.registerCGSCCAnalyses(cgam);
  pb.registerFunctionAnalyses(fam);
  pb.registerLoopAnalyses(lam);
  pb.crossRegisterProxies(lam, fam, cgam, mam);

  // 添加分析pass到管理器中
  mam.registerPass([]() { return StaticCallCounter(); });

#ifdef TASK4_LLM

  // 使用 LLM 技术来辅助编译优化
  // 初始化 Python 解释器
  Py::scoped_interpreter guard{};
  // import sys 库，添加 TASK4_DIR 到寻找 Python 库的 path 中
  Py::module_ sys = Py::module_::import("sys");
  sys.attr("path").attr("append")(TASK4_DIR);

  // 添加 LLM 加持的 Pass 到优化管理器中
  mpm.addPass(PassSequencePredict(
    "sk-IkaN52AW0UEoWBGXuRAlU88xiLRSebfOuvkWplVy9CgFAVkWCplle5Js1s0GpADA",
    "https://opencode.ai/zen/go/v1",
    {
      { "StaticCallCounterPrinter",
        TASK4_DIR "/StaticCallCounterPrinter.hpp",
        TASK4_DIR "/StaticCallCounterPrinter.cpp",
        "StaticCallCounterPrinter.xml",
        [](llvm::ModulePassManager& mpm) {
          mpm.addPass(StaticCallCounterPrinter(llvm::errs()));
        } },
      { "Mem2Reg",
        TASK4_DIR "/Mem2Reg.hpp",
        TASK4_DIR "/Mem2Reg.cpp",
        "Mem2Reg.xml",
        [](llvm::ModulePassManager& mpm) { mpm.addPass(Mem2Reg()); } },
      { "Inliner",
        TASK4_DIR "/Inliner.hpp",
        TASK4_DIR "/Inliner.cpp",
        "Inliner.xml",
        [](llvm::ModulePassManager& mpm) {
          mpm.addPass(Inliner(llvm::errs()));
        } },
      { "LICM",
        TASK4_DIR "/LICM.hpp",
        TASK4_DIR "/LICM.cpp",
        "LICM.xml",
        [](llvm::ModulePassManager& mpm) {
          mpm.addPass(LICM(llvm::errs()));
        } },
      { "LoopUnrolling",
        TASK4_DIR "/LoopUnrolling.hpp",
        TASK4_DIR "/LoopUnrolling.cpp",
        "LoopUnrolling.xml",
        [](llvm::ModulePassManager& mpm) {
          mpm.addPass(LoopUnrolling(llvm::errs()));
        } },
      { "ConstantPropagation",
        TASK4_DIR "/ConstantPropagation.hpp",
        TASK4_DIR "/ConstantPropagation.cpp",
        "ConstantPropagation.xml",
        [](llvm::ModulePassManager& mpm) {
          mpm.addPass(ConstantPropagation(llvm::errs()));
        } },
      { "ConstantFolding",
        TASK4_DIR "/ConstantFolding.hpp",
        TASK4_DIR "/ConstantFolding.cpp",
        "ConstantFolding.xml",
        [](llvm::ModulePassManager& mpm) {
          mpm.addPass(ConstantFolding(llvm::errs()));
        } },
      { "CSE",
        TASK4_DIR "/CSE.hpp",
        TASK4_DIR "/CSE.cpp",
        "CSE.xml",
        [](llvm::ModulePassManager& mpm) {
          mpm.addPass(CSE(llvm::errs()));
        } },
      { "DSE",
        TASK4_DIR "/DSE.hpp",
        TASK4_DIR "/DSE.cpp",
        "DSE.xml",
        [](llvm::ModulePassManager& mpm) {
          mpm.addPass(DSE(llvm::errs()));
        } },
      { "DCE",
        TASK4_DIR "/DCE.hpp",
        TASK4_DIR "/DCE.cpp",
        "DCE.xml",
        [](llvm::ModulePassManager& mpm) {
          mpm.addPass(DCE(llvm::errs()));
        } },
      { "InstructionCombining",
        TASK4_DIR "/InstructionCombining.hpp",
        TASK4_DIR "/InstructionCombining.cpp",
        "InstructionCombining.xml",
        [](llvm::ModulePassManager& mpm) {
          mpm.addPass(InstructionCombining(llvm::errs()));
        } },
      { "StrengthReduction",
        TASK4_DIR "/StrengthReduction.hpp",
        TASK4_DIR "/StrengthReduction.cpp",
        "StrengthReduction.xml",
        [](llvm::ModulePassManager& mpm) {
          mpm.addPass(StrengthReduction(llvm::errs()));
        } },
      { "AlgebraicIdentities",
        TASK4_DIR "/AlgebraicIdentities.hpp",
        TASK4_DIR "/AlgebraicIdentities.cpp",
        "AlgebraicIdentities.xml",
        [](llvm::ModulePassManager& mpm) {
          mpm.addPass(AlgebraicIdentities(llvm::errs()));
        } },
    }));

#else

  // 传统 LLVM Pass 来进行编译优化
  // 添加优化pass到管理器中
  mpm.addPass(StaticCallCounterPrinter(llvm::errs()));
  mpm.addPass(Mem2Reg());
  mpm.addPass(Inliner(llvm::errs()));
  mpm.addPass(LICM(llvm::errs()));
  mpm.addPass(LoopUnrolling(llvm::errs()));
  mpm.addPass(ConstantPropagation(llvm::errs()));
  mpm.addPass(ConstantFolding(llvm::errs()));
  mpm.addPass(CSE(llvm::errs()));
  mpm.addPass(DSE(llvm::errs()));
  mpm.addPass(DCE(llvm::errs()));
  mpm.addPass(InstructionCombining(llvm::errs()));
  mpm.addPass(StrengthReduction(llvm::errs()));
  mpm.addPass(AlgebraicIdentities(llvm::errs()));

#endif

  // 运行优化pass
  mpm.run(mod, mam);
}

int
main(int argc, char** argv)
{
  if (argc != 3) {
    std::cout << "Usage: " << argv[0] << " <input> <output>\n";
    return -1;
  }

  llvm::LLVMContext ctx;

  llvm::SMDiagnostic err;
  auto mod = llvm::parseIRFile(argv[1], err, ctx);
  if (!mod) {
    std::cout << "Error: unable to parse input file: " << argv[1] << '\n';
    err.print(argv[0], llvm::errs());
    return -2;
  }

  std::error_code ec;
  llvm::StringRef outPath(argv[2]);
  llvm::raw_fd_ostream outFile(outPath, ec);
  if (ec) {
    std::cout << "Error: unable to open output file: " << argv[2] << '\n';
    return -3;
  }

  opt(*mod); // IR的优化发生在这里

  mod->print(outFile, nullptr, false, true);
  if (llvm::verifyModule(*mod, &llvm::outs()))
    return 3;
}
