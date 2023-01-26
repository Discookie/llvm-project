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

// FIXME: Replace with structured binding
struct SanitizedLiteral {
  StringRef StrippedLiteral;
  // The exact bit value of the MSB
  std::size_t MSBBit;
  // The bit value of MSB rounded up to the nearest byte.
  std::size_t MSBByte;
  std::size_t Radix;
};

// Stripped literal, MSB position, radix of literal
// Does not calculate true MSB - only takes the value of the first digit into
// account alongside the total digit count. Returns MSB zero if radix is 10.
SanitizedLiteral sanitizeAndCountBits(std::string &IntegerLiteral) {
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
    return SanitizedLiteral { StrippedLiteral,
        StrippedLiteral.size(), 
        StrippedLiteral.size(), 
        2 };
  }
  else if(StrippedLiteral.consume_front("0x"))
  {
    StrippedLiteral = StrippedLiteral.take_while(llvm::isHexDigit);
    assert(!StrippedLiteral.empty());

    return { StrippedLiteral,
      (StrippedLiteral.size() - 1) * 4 + DigitMSB(StrippedLiteral.front()),
      StrippedLiteral.size() * 4,
      16 };
  }
  else if(StrippedLiteral != "0" && StrippedLiteral.consume_front("0"))
  {
    StrippedLiteral = StrippedLiteral.take_while(
      [](char c){ return c >= '0' || c <= '7'; });
    assert(!StrippedLiteral.empty());

    return { StrippedLiteral,
        (StrippedLiteral.size() - 1) * 3 + DigitMSB(StrippedLiteral.front()),
        StrippedLiteral.size() * 3,
        8 };
  } else {
    return { StrippedLiteral, 0, 0, 10 };
  }
}

void NonPortableIntegerConstantCheck::registerMatchers(MatchFinder *Finder) {
  Finder->addMatcher(integerLiteral().bind("integer"), this);
}


void NonPortableIntegerConstantCheck::check(
    const MatchFinder::MatchResult &Result) {
  const auto *MatchedInt = Result.Nodes.getNodeAs<IntegerLiteral>("integer");

  assert(MatchedInt);

  QualType IntegerLiteralType = MatchedInt->getType();
  auto LiteralBitWidth = Result.Context->getTypeSize( IntegerLiteralType );

  llvm::APInt LiteralValue = MatchedInt->getValue();

  std::string LiteralStr =
      Lexer::getSourceText(
          CharSourceRange::getTokenRange(MatchedInt->getSourceRange()),
          *Result.SourceManager, Result.Context->getLangOpts(), nullptr)
          .lower();

  // FIXME: There are two problematic cases where we cannot read the character.
  // With macros, in some cases (such as when not passing an argument) the
  // integer literal's token range will be 0 long.
  if (LiteralStr.size() == 0)
    return;
  // FIXME: A template function with an integer literal template argument will
  // warn in both the argument, and the function body. In the instantiated body,
  // the source range will contain the argument name, not the literal.
  if (!llvm::isDigit(LiteralStr[0]))
    return;

  auto SanitizedLiteral =
      sanitizeAndCountBits(LiteralStr);

  // Only potential edge case is "0", handled by sanitizeAndCountBits.
  assert(!SanitizedLiteral.StrippedLiteral.empty() &&
         "integer literal should not be empty");

  assert(SanitizedLiteral.MSBBit <= LiteralBitWidth &&
         "integer literal has more bits set than its bit width");

  bool IsMax = LiteralValue.isMaxValue() || LiteralValue.isMaxSignedValue();
  bool IsUnsignedMaxMinusOne = (LiteralValue + 1).isMaxValue();
  bool IsMin = LiteralValue.isMinValue() || LiteralValue.isMinSignedValue();
  bool RepresentsZero = LiteralValue.isNullValue();

  bool IsFullPattern = SanitizedLiteral.MSBBit == LiteralBitWidth;
  // Can be greater, eg. an 8-bit UCHAR_MAX byte value represented by 377 octal
  bool IsFullPatternAlternate = SanitizedLiteral.MSBByte >= LiteralBitWidth;
  bool HasLeadingZeroes = SanitizedLiteral.StrippedLiteral[0] == '0';

  if (IsMax || IsUnsignedMaxMinusOne) {
    diag(MatchedInt->getBeginLoc(),
          "error-prone literal: do not hardcode integer maximum value");
  } else if (IsMin && !RepresentsZero) {
    diag(MatchedInt->getBeginLoc(),
          "error-prone literal: do not hardcode integer minimum value");
  } else if (HasLeadingZeroes && !RepresentsZero) {
    diag(MatchedInt->getBeginLoc(),
         "error-prone literal: integer literal has leading zeroes");
  // Matches only the most significant bit,
  // eg. unsigned value 0x80000000.
  } else if (IsFullPattern) {
    diag(MatchedInt->getBeginLoc(),
         "error-prone literal: should not rely on the most significant bit");
  // This warning also matches literals like 0x30000000,
  // for statistics purposes for now.
  } else if (IsFullPatternAlternate) {
    diag(MatchedInt->getBeginLoc(),
      "error-prone literal: should not rely on bits of most significant byte");
  }
}

} // namespace portability
} // namespace tidy
} // namespace clang
