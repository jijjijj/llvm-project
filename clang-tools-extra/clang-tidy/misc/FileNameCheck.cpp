//===--- ImportOrderCheck.cpp - clang-tidy -------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "FileNameCheck.h"
#include "clang/Frontend/CompilerInstance.h"
#include "llvm/ADT/STLExtras.h"
#include "clang/Tooling/FixIt.h"
#include "clang/Lex/Lexer.h"

#include <map>
#include <iostream>
#include <sstream>

namespace clang::tidy::misc {

namespace {

} // namespace

void FileNameCheck::registerMatchers(ast_matchers::MatchFinder *Finder) {
  Finder->addMatcher(ast_matchers::importDecl().bind(ImportId),
    this);
}

void FileNameCheck::check(
    const ast_matchers::MatchFinder::MatchResult &Result) {
  const auto *Import = Result.Nodes.getNodeAs<ImportDecl>(ImportId);

  if (!Import)
    return;


}

void FileNameCheck::onEndOfTranslationUnit() {
  if (Imports.empty())
    return;

}

void FileNameCheck::storeOptions(ClangTidyOptions::OptionMap &Opts) {
  Options.store(Opts, UserModuleRegexOptionName, DefaultUserModuleRegex);
}

FileNameCheck::FileNameCheck(StringRef Name, ClangTidyContext *Context)
    : ClangTidyCheck(Name, Context)
    , UserModuleRegex{
        Options.get(UserModuleRegexOptionName, DefaultUserModuleRegex)} {}
} // namespace clang::tidy::misc
