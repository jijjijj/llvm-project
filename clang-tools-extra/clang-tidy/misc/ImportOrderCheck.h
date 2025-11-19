//===--- ImportOrderCheck.h - clang-tidy -----------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_TOOLS_EXTRA_CLANG_TIDY_MISC_IMPORTORDERCHECK_H
#define LLVM_CLANG_TOOLS_EXTRA_CLANG_TIDY_MISC_IMPORTORDERCHECK_H

#include "../ClangTidyCheck.h"

namespace clang::tidy::misc {

/// Checks the correct order of `imports`.
///
/// Priorities:
/// 1. import std;
/// 2 . import std.*;
/// 3. import [^:]*;
/// 4. import X.*; <- X is configurable
/// 5. import :*;

class ImportOrderCheck : public ClangTidyCheck {
public:
  ImportOrderCheck(StringRef Name, ClangTidyContext *Context);
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

#endif // LLVM_CLANG_TOOLS_EXTRA_CLANG_TIDY_MISC_IMPORTORDERCHECK_H
