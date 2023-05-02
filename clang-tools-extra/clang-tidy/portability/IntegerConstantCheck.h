//===--- IntegerConstantCheck.h - clang-tidy --------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_TOOLS_EXTRA_CLANG_TIDY_PORTABILITY_NON_PORTABLE_INTEGER_CONSTANT_CHECK_H
#define LLVM_CLANG_TOOLS_EXTRA_CLANG_TIDY_PORTABILITY_NON_PORTABLE_INTEGER_CONSTANT_CHECK_H

#include "../ClangTidyCheck.h"

namespace clang::tidy::portability {

/// Finds integer literals that are being used in a non-portable manner.
///
/// For the user-facing documentation see:
/// http://clang.llvm.org/extra/clang-tidy/checks/portability/integer-constant.html
class IntegerConstantCheck : public ClangTidyCheck {
public:
  IntegerConstantCheck(StringRef Name, ClangTidyContext *Context)
      : ClangTidyCheck(Name, Context) {}
  void registerMatchers(ast_matchers::MatchFinder *Finder) override;
  void check(const ast_matchers::MatchFinder::MatchResult &Result) override;
};

} // namespace clang::tidy::portability

#endif // LLVM_CLANG_TOOLS_EXTRA_CLANG_TIDY_PORTABILITY_NON_PORTABLE_INTEGER_CONSTANT_CHECK_H
