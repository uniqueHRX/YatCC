#include "PassSequencePredict.hpp"
#include "LLMHelper.hpp"
#include <filesystem>
#include <functional>
#include <llvm/ADT/StringRef.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/PassManager.h>
#include <llvm/Pass.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/raw_ostream.h>
#include <pybind11/pybind11.h>
#include <pybind11/pytypes.h>
#include <unordered_map>

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

void
write_file(llvm::StringRef filePath, llvm::StringRef content)
{
  std::error_code ec;
  llvm::raw_fd_ostream outFile(filePath, ec);
  if (ec) {
    llvm::errs() << "无法打开文件：" << filePath << '\n';
    std::abort();
  }

  outFile << content;
}

} // namespace

std::string
PassSequencePredict::pass_summary(PassSequencePredict::PassInfo& passInfo)
{
  // 判断是否存在输出文件以及其是否过时
  // 对比缓存内容、头文件和实现文件的最后修改时间
  if (Fs::exists(passInfo.mSummaryPath)) {
    auto hppLastWriteTime = Fs::last_write_time(passInfo.mHppPath);
    auto cppLastWriteTime = Fs::last_write_time(passInfo.mCppPath);
    auto summaryLastWriteTime = Fs::last_write_time(passInfo.mSummaryPath);

    if (summaryLastWriteTime >= hppLastWriteTime &&
        summaryLastWriteTime >= cppLastWriteTime) {
      return read_file(passInfo.mSummaryPath);
    }
  }

  // 调用 LLM
  // 读取预先存储在文件中的提示词
  std::string systemPrompt =
    read_file(TASK4_DIR "/llm/prompts/PassSummarySysPrTpl.xml");
  auto userPrompt =
    Py::str(read_file(TASK4_DIR "/llm/prompts/PassSummaryUserPrTpl.xml"))
      .attr("format")("name"_a = passInfo.mClassName,
                      "hpp"_a = read_file(passInfo.mHppPath),
                      "cpp"_a = read_file(passInfo.mCppPath))
      .cast<std::string>();

  // 创建会话
  std::string sessionID = mHelper.create_new_session();
  mHelper.add_content(sessionID, Role::kSystem, systemPrompt);
  mHelper.add_content(sessionID, Role::kUser, userPrompt);

  Py::module_ llm = Py::module_::import("llm");
  Py::list handlers;
  handlers.append(llm.attr("remove_deepseek_r1_think"));
  handlers.append(llm.attr("remove_md_block_marker")("xml"));
  std::string response = mHelper.chat(
    sessionID,
    "deepseek-v4-flash",
    handlers,
    Py::dict("max_tokens"_a = 8192, "stream"_a = false, "temperature"_a = 0));

  write_file(passInfo.mSummaryPath, response);

  // 删除会话
  mHelper.delete_session(sessionID);
  return response;
}

PassSequencePredict::PassSequencePredict(
  llvm::StringRef apiKey,
  llvm::StringRef baseURL,
  std::initializer_list<PassInfo> passesInfo)
  : mHelper(apiKey, baseURL)
  , mPassesInfo(passesInfo)
{
}

llvm::PreservedAnalyses
PassSequencePredict::run(llvm::Module& mod, llvm::ModuleAnalysisManager& mam)
{
  // 生成 pass summary
  std::string passSummary;
  for (auto& passLocation : mPassesInfo) {
    passSummary.append(pass_summary(passLocation));
  }

  // 将 LLVM::Module 转换为字符串，发送给大模型进行 IR 的分析
  std::string module;
  llvm::raw_string_ostream os(module);
  mod.print(os, nullptr, false, true);
  os.flush();

  // 读取提示词
  std::string systemPrompt =
    read_file(TASK4_DIR "/llm/prompts/PassSeqPredSysPrTpl.xml");
  auto userPrompt =
    Py::str(read_file(TASK4_DIR "/llm/prompts/PassSeqPredUserPrTpl.xml"))
      .attr("format")("ir"_a = module, "passes"_a = passSummary)
      .cast<std::string>();

  // 创建 LLM 会话
  auto sessionID = mHelper.create_new_session();
  mHelper.add_content(sessionID, Role::kSystem, systemPrompt);
  mHelper.add_content(sessionID, Role::kUser, userPrompt);

  Py::module_ llm = Py::module_::import("llm");

  // 第一步：获取 LLM 原始响应，仅做清理（去除 think 和 markdown 包裹）
  Py::list cleanHandlers;
  cleanHandlers.append(llm.attr("remove_deepseek_r1_think"));
  cleanHandlers.append(llm.attr("remove_md_block_marker")("xml"));
  std::string cleanResponse = mHelper.chat(
    sessionID,
    "deepseek-v4-flash",
    cleanHandlers,
    Py::dict("max_tokens"_a = 25600, "temperature"_a = 0.1, "stream"_a = false));

  // 第二步：从清理后的 XML 中分别提取 reasoning 和 sequence
  std::string reasoning =
    llm.attr("extract_xml_tag")(cleanResponse, "reasoning")
      .cast<std::string>();
  std::string sequence =
    llm.attr("extract_xml_tag")(cleanResponse, "sequence")
      .cast<std::string>();

  // 输出 LLM 的推理过程到日志，提供可解释性
  llvm::errs() << "\n========================================\n";
  llvm::errs() << "LLM Pass 序列预测分析\n";
  llvm::errs() << "========================================\n";
  llvm::errs() << reasoning << "\n";
  llvm::errs() << "----------------------------------------\n";
  llvm::errs() << "预测序列: " << sequence << "\n";
  llvm::errs() << "========================================\n\n";

  // 将字符串形式的 pass sequence 转换为 Py::list
  auto passSequence = Py::str(sequence).attr("split")(",").cast<Py::list>();

  // 定义 pass 类名到 <向 ModulePassManager 添加其实例的函数> 的映射
  std::unordered_map<std::string, std::function<void(llvm::ModulePassManager&)>>
    map;
  for (auto& passInfo : mPassesInfo) {
    map[passInfo.mClassName] = passInfo.mAddPass;
  }

  // 向 mpm 中添加 pass
  llvm::ModulePassManager mpm;
  for (auto& passClassName : passSequence) {
    map[passClassName.cast<std::string>()](mpm);
  }
  mpm.run(mod, mam);

  // 删除会话
  mHelper.delete_session(sessionID);

  // 不需要保留任何分析结果
  return llvm::PreservedAnalyses::none();
}
