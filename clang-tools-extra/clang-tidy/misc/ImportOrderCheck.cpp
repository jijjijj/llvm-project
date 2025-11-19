//===--- ImportOrderCheck.cpp - clang-tidy -------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "ImportOrderCheck.h"
#include "clang/Frontend/CompilerInstance.h"
#include "llvm/ADT/STLExtras.h"
#include "clang/Tooling/FixIt.h"
#include "clang/Lex/Lexer.h"

#include <map>
#include <iostream>
#include <sstream>

namespace clang::tidy::misc {

namespace {
  constexpr const char* DefaultUserModuleRegex = "";
  constexpr const char* UserModuleRegexOptionName = "UserModuleRegex";
  constexpr const char* ImportId = "import";
  constexpr const char* ModuleStd = "std";
  constexpr const char* ModuleStartsWithStd = "std.";
  constexpr char ModulePartitionSymbol = ':';

  int getPriority(StringRef ModuleName, const llvm::Regex& UserModuleRegex, bool IsPartition) {
    // import std
    if (ModuleName == ModuleStd)
      return 0;

    // import std.*, like std.compat
    if (ModuleName.starts_with(ModuleStartsWithStd))
      return 1;

    // import UserModuleRegexp
    if (UserModuleRegex.match(ModuleName))
      return 3;

    // partitions are last;
    if (IsPartition)
      return 4;

    // Other imports are between std.* and UserModuleRegexp
    return 2;
  }

  SourceRange getImportDeclRange(const ImportDecl& Decl,
                                 const clang::SourceManager& SM) {
    clang::LangOptions Lo;

    const SourceRange& Range = Decl.getSourceRange();
    // NOTE: sm.getSpellingLoc() used in case the range corresponds to a
    // macro/preprocessed source.
    auto StartLoc = SM.getSpellingLoc(Range.getBegin());
    auto LastTokenLoc = SM.getSpellingLoc(Range.getEnd());
    std::optional<Token> Tok;
    while ((Tok = clang::Lexer::findNextToken(LastTokenLoc, SM, Lo)) &&
           !Tok->is(tok::semi)) {
      LastTokenLoc = Tok->getLocation();
    }

    return SourceRange{ StartLoc, Tok->getEndLoc() };
  }
} // namespace

void ImportOrderCheck::registerMatchers(ast_matchers::MatchFinder *Finder) {
  Finder->addMatcher(ast_matchers::importDecl().bind(ImportId),
    this);
}

void ImportOrderCheck::check(
    const ast_matchers::MatchFinder::MatchResult &Result) {
  const auto *Import = Result.Nodes.getNodeAs<ImportDecl>(ImportId);

  if (!Import)
    return;

  const std::string& Name = Import->getImportedModule()->Name;

  // must the "module X;" declaration. Should not be sorted
  const Module* OwningModule = Import->getOwningModule();
  if (OwningModule && Name == OwningModule->Name)
    return;

  if (!SourceManager)
    SourceManager = *Result.SourceManager;
  if (!ASTContext)
    ASTContext = *Result.Context;

  size_t Index = Name.find(ModulePartitionSymbol);
  const bool IsPartition = Index != std::string::npos;

  ImportInfo Info = {
      IsPartition ? Name.substr(Index) : Name,
      getPriority(Name, UserModuleRegex, IsPartition),
      Import->isInExportDeclContext(),
      getImportDeclRange(*Import, SourceManager->get())
  };

  Imports.push_back(std::move(Info));
}

void ImportOrderCheck::onEndOfTranslationUnit() {
  if (Imports.empty())
    return;

  // TODO: find duplicated includes.

  // Form blocks of includes. We don't want to sort across blocks. This also
  // implicitly makes us never reorder over #defines or #if directives.
  // FIXME: We should be more careful about sorting below comments as we don't
  // know if the comment refers to the next include or the whole block that
  // follows.

  const clang::SourceManager& SM = SourceManager->get();
  const clang::ASTContext& Context = ASTContext->get();

  std::vector<unsigned> Blocks(1, 0);
  for (unsigned I = 1, E = Imports.size(); I != E; ++I)
    if (SM.getExpansionLineNumber(Imports[I].Range.getBegin()) !=
        SM.getExpansionLineNumber(Imports[I - 1].Range.getEnd()) + 1)
      Blocks.push_back(I);
  Blocks.push_back(Imports.size()); // Sentinel value.

  // Get a vector of indices.
  std::vector<unsigned> ImportIndices;
  for (unsigned I = 0, E = Imports.size(); I != E; ++I)
    ImportIndices.push_back(I);

  // Sort the imports. We first sort by priority, then lexicographically.
  for (unsigned BI = 0, BE = Blocks.size() - 1; BI != BE; ++BI)
    llvm::sort(ImportIndices.begin() + Blocks[BI],
               ImportIndices.begin() + Blocks[BI + 1],
               [this](unsigned LHSI, unsigned RHSI) {
                 ImportInfo &LHS = Imports[LHSI];
                 ImportInfo &RHS = Imports[RHSI];

                 int PriorityLHS = LHS.Priority;
                 int PriorityRHS = RHS.Priority;

                 return std::tie(PriorityLHS, LHS.ModuleName) <
                        std::tie(PriorityRHS, RHS.ModuleName);
               });

  // Emit a warning for each block and fixits for all changes within that
  // block.
  for (unsigned BI = 0, BE = Blocks.size() - 1; BI != BE; ++BI) {
    // Find the first import that's not in the right position.
    unsigned I = 0, E = 0;
    for (I = Blocks[BI], E = Blocks[BI + 1]; I != E; ++I)
      if (ImportIndices[I] != I)
        break;

    if (I == E)
      continue;

    // Emit a warning.
    std::stringstream Stream;
    for (unsigned J = I; J < E; ++J) {
      if (J != I)
        Stream << ", ";

      Stream << Imports[ImportIndices[J]].ModuleName;
    }
    auto D = diag(Imports[I].Range.getBegin(),
                  "imports are not sorted properly. Should be: " +
                      Stream.str());

    // Emit fix-its for all following imports in this block.
    for (; I != E; ++I) {
      if (ImportIndices[I] == I)
        continue;
      const ImportInfo &CopyFrom = Imports[ImportIndices[I]];

      D << FixItHint::CreateReplacement(Imports[I].Range,
                                        clang::tooling::fixit::getText(
                                            CopyFrom.Range, Context));
    }
  }

  Imports.clear();
  SourceManager.reset();
  ASTContext.reset();
}

void ImportOrderCheck::storeOptions(ClangTidyOptions::OptionMap &Opts) {
  Options.store(Opts, UserModuleRegexOptionName, DefaultUserModuleRegex);
}

ImportOrderCheck::ImportOrderCheck(StringRef Name, ClangTidyContext *Context)
    : ClangTidyCheck(Name, Context)
    , UserModuleRegex{
        Options.get(UserModuleRegexOptionName, DefaultUserModuleRegex)} {}
} // namespace clang::tidy::misc
