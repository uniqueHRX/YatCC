#pragma once

#include <llvm/IR/Dominators.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/PassManager.h>
#include <llvm/ADT/Hashing.h>
#include <llvm/ADT/DenseMap.h>
#include <llvm/Support/raw_ostream.h>

class CSE : public llvm::PassInfoMixin<CSE>
{
public:
  explicit CSE(llvm::raw_ostream& out)
    : mOut(out)
  {
  }

  llvm::PreservedAnalyses run(llvm::Module& mod,
                              llvm::ModuleAnalysisManager& mam);

private:
  llvm::raw_ostream& mOut;
};
