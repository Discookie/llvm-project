//===--- NonPortableIntegerConstantCheck.cpp - clang-tidy -----------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "NonPortableIntegerConstantCheck.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/Decl.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/Lex/Lexer.h"

using namespace clang::ast_matchers;

namespace clang {
namespace tidy {
namespace portability {

void NonPortableIntegerConstantCheck::registerMatchers(MatchFinder *Finder) {
  Finder->addMatcher(integerLiteral().bind("integer"), this);
}


void NonPortableIntegerConstantCheck::check(
    const MatchFinder::MatchResult &Result) {
  const auto *MatchedInt = Result.Nodes.getNodeAs<IntegerLiteral>("integer");

  assert(MatchedInt);

  std::string LiteralStr =
      Lexer::getSourceText(
          CharSourceRange::getTokenRange(MatchedInt->getSourceRange()),
          *Result.SourceManager, Result.Context->getLangOpts(), nullptr)
          .lower();

  llvm::APInt LiteralValue = MatchedInt->getValue();



  llvm::erase_value(LiteralStr, '\''); // Skip digit separators.
  StringRef StrippedLiteral{LiteralStr};
  bool IsFullPattern = false;

  QualType IntegerLiteralType = MatchedInt->getType();
  unsigned int IntegerLiteralTypeSize = Result.Context->getTypeSize( IntegerLiteralType );

  if(StrippedLiteral.consume_front("0b"))
  {
    StrippedLiteral = StrippedLiteral.take_while([](char c){ return c == '0' || c == '1'; });
    if(IntegerLiteralTypeSize == StrippedLiteral.size())
        IsFullPattern = true;
    
  }
  else if(StrippedLiteral.consume_front("0x"))
  {
    StrippedLiteral = StrippedLiteral.take_while(llvm::isHexDigit);
    const unsigned StrippedLiteralSizeInBits = StrippedLiteral.size() * 4; // Hexadecimal to binary.
    if(IntegerLiteralTypeSize == StrippedLiteralSizeInBits)
        IsFullPattern = true;
  }
  else if(StrippedLiteral.consume_front("0")) // not yet done
  {
    StrippedLiteral = StrippedLiteral.take_while(llvm::isHexDigit);
    // if(IntegerLiteralTypeSize == StrippedLiteral.size()*4)
        // IsFullPattern = true;
  }

  // for testing purpose
  llvm::errs() << "--------------------------------" << "\n\n"
              << "\n LiteralStr: " << LiteralStr << "\n APINT: " << LiteralValue
               << "  StrippedLiteral:" << StrippedLiteral
               << "  str size:" << StrippedLiteral.size() << "\n\n"
               << "  Type: " << IntegerLiteralType.getAsString() << "\n\n"
               << "  Size: " << IntegerLiteralTypeSize << "\n\n"
               << "  Fullpattern: " << IsFullPattern << "\n\n"
               << "  max: " << llvm::APInt::getMaxValue(IntegerLiteralTypeSize ) << "\n\n"
               << "  max signed: " << llvm::APInt::getSignedMaxValue(IntegerLiteralTypeSize ) << "\n\n"
               << "  IsMax: " << LiteralValue.isMaxValue() << "\n\n"
               << "  IsMaxSigned: " << LiteralValue.isMaxSignedValue() << "\n\n"
               << "  IsMax -1 : " << (MatchedInt->getValue()+1).isMaxValue() << "\n\n"
               << "  IsMaxSigned -1 : " << (MatchedInt->getValue()+1).isMaxSignedValue() << "\n\n"
               << "  IsMin: " << LiteralValue.isMinValue() << "\n\n"
               << "  IsMinSigned: " << LiteralValue.isMinSignedValue() << "\n\n"
               << "--------------------------------" << "\n\n";


  if (StrippedLiteral.empty())
    return;

  const bool RepresentsZero = LiteralValue.isNullValue();
  const bool HasLeadingZeroes = StrippedLiteral[0] == '0';
  const bool IsMax = LiteralValue.isMaxValue() || LiteralValue.isMaxSignedValue();
  const bool IsMin = LiteralValue.isMinValue() || LiteralValue.isMinSignedValue();
  const bool IsUnsignedMax_1 = (MatchedInt->getValue()+1).isMaxValue();

  const bool IntegralPattern =
      (HasLeadingZeroes && !RepresentsZero) || IsFullPattern || IsMax || (IsMin && !RepresentsZero) || IsUnsignedMax_1;

  if (IntegralPattern)
    diag(MatchedInt->getBeginLoc(),
         "integer is being used in a non-portable manner");
}

} // namespace portability
} // namespace tidy
} // namespace clang
