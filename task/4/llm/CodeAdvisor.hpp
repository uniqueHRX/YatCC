#pragma once

#include "LLMHelper.hpp"
#include <functional>
#include <llvm/ADT/StringRef.h>
#include <llvm/IR/PassManager.h>
#include <string>
#include <vector>

class CodeAdvisor : public llvm::PassInfoMixin<CodeAdvisor>
{
public:
  struct PassInfo
  {
    std::string mClassName;
    std::string mHppPath;
    std::string mCppPath;
    std::string mSummaryPath;
    std::function<void(llvm::ModulePassManager&)> mAddPass;
  };

  CodeAdvisor(llvm::StringRef apiKey,
              llvm::StringRef baseURL,
              std::vector<PassInfo> passesInfo);

  llvm::PreservedAnalyses run(llvm::Module& mod,
                              llvm::ModuleAnalysisManager& mam);

private:
  LLMHelper mHelper;
  std::vector<PassInfo> mPassesInfo;
};