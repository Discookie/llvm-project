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

// Stripped literal, MSB position, radix of literal
// Does not calculate true MSB - only takes the value of the first digit into
// account alongside the total digit count. Returns MSB zero if radix is 10.
std::tuple<StringRef, uint64_t, int> sanitizeAndCountBits(std::string &IntegerLiteral) {
  llvm::erase_value(IntegerLiteral, '\''); // Skip digit separators.
  StringRef StrippedLiteral{IntegerLiteral};

  const auto DigitMSB = [](char digit) {
    if (digit == '0') {
      return 0;
    } else if (digit <= '1') {
      return 1;
    } else if (digit <= '3') {
      return 2;
    } else if (digit <= '7') {
      return 3;
    } else {
      return 4;
    }
  };

  if(StrippedLiteral.consume_front("0b"))
  {
    StrippedLiteral = StrippedLiteral.take_while(
      [](char c){return c == '0' || c == '1'; });
    return std::tuple { StrippedLiteral, StrippedLiteral.size(), 2 };
  }
  else if(StrippedLiteral.consume_front("0x"))
  {
    StrippedLiteral = StrippedLiteral.take_while(llvm::isHexDigit);
    return std::tuple { StrippedLiteral,
        (StrippedLiteral.size() - 1) * 4 + DigitMSB(StrippedLiteral.front()),
        16 };
  }
  else if(StrippedLiteral != "0" && StrippedLiteral.consume_front("0"))
  {
    StrippedLiteral = StrippedLiteral.take_while(
      [](char c){ return c >= '0' || c <= '7'; });
    return std::tuple { StrippedLiteral,
        (StrippedLiteral.size() - 1) * 3 + DigitMSB(StrippedLiteral.front()),
        8 };
  } else {
    return std::tuple { StrippedLiteral, 0, 10 };
  }
}

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

  auto [ StrippedLiteral, LiteralMSB, LiteralRadix ] = sanitizeAndCountBits(
    LiteralStr
  );
  QualType IntegerLiteralType = MatchedInt->getType();
  unsigned int LiteralBitWidth = Result.Context->getTypeSize( IntegerLiteralType );


  // for testing purpose
  llvm::errs() << "--------------------------------" << "\n\n"
              << "\n LiteralStr: " << LiteralStr << "\n APINT: " << LiteralValue
               << "  StrippedLiteral:" << StrippedLiteral
               << "  str size:" << StrippedLiteral.size() << "\n\n"
               << "  Type: " << IntegerLiteralType.getAsString() << "\n\n"
               << "  Size: " << LiteralBitWidth << "\n\n"
               << "  MSB: " << LiteralMSB << "\n\n"
               << "  Radix: " << LiteralRadix << "\n\n"
               << "  max: " << llvm::APInt::getMaxValue(LiteralBitWidth ) << "\n\n"
               << "  max signed: " << llvm::APInt::getSignedMaxValue(LiteralBitWidth ) << "\n\n"
               << "  IsMax: " << LiteralValue.isMaxValue() << "\n\n"
               << "  IsMaxSigned: " << LiteralValue.isMaxSignedValue() << "\n\n"
               << "  IsMax -1 : " << (MatchedInt->getValue()+1).isMaxValue() << "\n\n"
               << "  IsMaxSigned -1 : " << (MatchedInt->getValue()+1).isMaxSignedValue() << "\n\n"
               << "  IsMin: " << LiteralValue.isMinValue() << "\n\n"
               << "  IsMinSigned: " << LiteralValue.isMinSignedValue() << "\n\n"
               << "--------------------------------" << "\n\n";

  // Only potential edge case is "0", handled by sanitizeAndCountBits.
  assert(!StrippedLiteral.empty() && "integer literal should not be empty");

  const bool IsFullPattern = LiteralMSB == LiteralBitWidth;
  const bool RepresentsZero = LiteralValue.isNullValue();
  const bool HasLeadingZeroes = StrippedLiteral[0] == '0';
  const bool IsMax = LiteralValue.isMaxValue() || LiteralValue.isMaxSignedValue();
  const bool IsMin = LiteralValue.isMinValue() || LiteralValue.isMinSignedValue();
  const bool IsUnsignedMax_1 = (LiteralValue + 1).isMaxValue();

  if (IsFullPattern) {
    diag(MatchedInt->getBeginLoc(),
         "error-prone literal: should not rely on the most significant bit",
         DiagnosticIDs::Note);
  }  

  if (HasLeadingZeroes && !RepresentsZero) {
    diag(MatchedInt->getBeginLoc(),
         "integer literal has leading zeroes", DiagnosticIDs::Note);
  }  

  if (IsMax || IsUnsignedMax_1) {
    diag(MatchedInt->getBeginLoc(),
         "do not hardcode integer maximum value", DiagnosticIDs::Note);
  }  

  if (IsMin && !RepresentsZero) {
    diag(MatchedInt->getBeginLoc(),
         "do not hardcode integer minimum value", DiagnosticIDs::Note);
  }

  const bool IntegralPattern =
      (HasLeadingZeroes && !RepresentsZero) || IsFullPattern || IsMax || (IsMin && !RepresentsZero) || IsUnsignedMax_1;

  if (IntegralPattern)
    diag(MatchedInt->getBeginLoc(),
         "integer is being used in a non-portable manner");
}

} // namespace portability
} // namespace tidy
} // namespace clang
