//===--- ImportOrderCheck.h - clang-tidy -----------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_TOOLS_EXTRA_CLANG_TIDY_MISC_FILENAMECHECK_H
#define LLVM_CLANG_TOOLS_EXTRA_CLANG_TIDY_MISC_FILENAMECHECK_H

#include "../ClangTidyCheck.h"

namespace clang::tidy::misc {

/// Checks whether files have proper names. Extensions are not checked.
/// The rules are (in order of decreasing priority):
///
/// 1. Files with main module interface: module.cppm
/// 2. Files with the main function: main.cpp
/// 3. Files with tests (?): (snake_case)_tests postfix
/// 4. Files with benchmarks (?): (snake_case)_benchmarks
/// 5. Module's interface partition filename should always match that partition
/// name
/// 6. Files that contain at least one class/enum/struct/union in non-anonymous
/// namespace: the filename should match one of those classes/enums/structs/
/// unions
///

class FileNameCheck : public ClangTidyCheck {
public:
  FileNameCheck(StringRef Name, ClangTidyContext *Context);
  void registerMatchers(ast_matchers::MatchFinder *Finder) override;
  void check(const ast_matchers::MatchFinder::MatchResult &Result) override;
  void onEndOfTranslationUnit() override;
  void storeOptions(ClangTidyOptions::OptionMap &Opts) override;

private:
  struct ImportInfo {
    std::string ModuleName;
    int Priority = std::numeric_limits<int>::max();
    bool IsExport = false;
    SourceRange Range{};
  };

  llvm::Regex UserModuleRegex;
  std::vector<ImportInfo> Imports;
  std::optional<std::reference_wrapper<SourceManager>> SourceManager;
  std::optional<std::reference_wrapper<ASTContext>> ASTContext;
};

} // namespace clang::tidy::misc

#endif // LLVM_CLANG_TOOLS_EXTRA_CLANG_TIDY_MISC_FILENAMECHECK_H
