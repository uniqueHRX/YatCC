#include "CodeAdvisor.hpp"
#include <filesystem>
#include <llvm/ADT/StringRef.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/raw_ostream.h>
#include <pybind11/pybind11.h>
#include <pybind11/pytypes.h>

namespace Fs = std::filesystem;
namespace Py = pybind11;
using namespace Py::literals;
using Role = LLMHelper::Role;

namespace {

std::string
read_file(llvm::StringRef filePath)
{
  auto inFileOrErr = llvm::MemoryBuffer::getFile(filePath);
  if (auto err = inFileOrErr.getError()) {
    llvm::errs() << "无法读取文件：" << filePath << '\n';
    std::abort();
  }
  return std::move(inFileOrErr.get()->getBuffer().str());
}

} // namespace

CodeAdvisor::CodeAdvisor(llvm::StringRef apiKey,
                         llvm::StringRef baseURL,
                         std::vector<PassInfo> passesInfo)
  : mHelper(apiKey, baseURL)
  , mPassesInfo(std::move(passesInfo))
{
}

llvm::PreservedAnalyses
CodeAdvisor::run(llvm::Module& mod, llvm::ModuleAnalysisManager& mam)
{
  // 收集所有 Pass 的摘要（复用已有的缓存文件）
  std::string passSummary;
  for (auto& passInfo : mPassesInfo) {
    if (Fs::exists(passInfo.mSummaryPath)) {
      passSummary.append(read_file(passInfo.mSummaryPath));
    }
  }

  // 将优化后的 IR 序列化为字符串
  std::string optimizedIR;
  llvm::raw_string_ostream os(optimizedIR);
  mod.print(os, nullptr, false, true);
  os.flush();

  // 读取提示词
  std::string systemPrompt =
    read_file(TASK4_DIR "/llm/prompts/AdvisorSysPrTpl.xml");
  auto userPrompt =
    Py::str(read_file(TASK4_DIR "/llm/prompts/AdvisorUserPrTpl.xml"))
      .attr("format")("passes"_a = passSummary, "ir"_a = optimizedIR)
      .cast<std::string>();

  // 创建 LLM 会话
  auto sessionID = mHelper.create_new_session();
  mHelper.add_content(sessionID, Role::kSystem, systemPrompt);
  mHelper.add_content(sessionID, Role::kUser, userPrompt);

  // 清理响应
  Py::module_ llm = Py::module_::import("llm");
  Py::list handlers;
  handlers.append(llm.attr("remove_deepseek_r1_think"));
  handlers.append(llm.attr("remove_md_block_marker")("xml"));
  std::string cleanResponse = mHelper.chat(
    sessionID,
    "deepseek-v4-flash",
    handlers,
    Py::dict("max_tokens"_a = 25600, "temperature"_a = 0, "stream"_a = false));

  // 提取 assessment 和 suggestions（均为纯文本标签）
  std::string assessment =
    llm.attr("extract_xml_tag")(cleanResponse, "assessment")
      .cast<std::string>();
  std::string suggestions =
    llm.attr("extract_xml_tag")(cleanResponse, "suggestions")
      .cast<std::string>();

  // 输出到日志
  llvm::errs() << "\n========================================\n";
  llvm::errs() << "代码开发助手 — 优化建议\n";
  llvm::errs() << "========================================\n";
  llvm::errs() << "整体评估: " << assessment << "\n";
  llvm::errs() << "----------------------------------------\n";
  llvm::errs() << "优化建议:\n" << suggestions << "\n";
  llvm::errs() << "========================================\n\n";

  mHelper.delete_session(sessionID);

  return llvm::PreservedAnalyses::all();
}