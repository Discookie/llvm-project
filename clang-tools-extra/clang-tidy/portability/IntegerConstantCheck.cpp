//===--- IntegerConstantCheck.cpp - clang-tidy ----------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "IntegerConstantCheck.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/Decl.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/Lex/Lexer.h"

using namespace clang::ast_matchers;

namespace clang::tidy::portability {
namespace {

struct SanitizedLiteralType {
  StringRef StrippedLiteral;
  // The exact bit value of the MSB.
  std::size_t MSBBit;
  // The bit value of MSB rounded up to the nearest byte.
  std::size_t MSBByte;
  std::size_t Radix;
};

} // namespace

// Does not calculate true MSB - only takes the value of the first digit into
// account alongside the total digit count. Returns MSB zero if radix is 10, and
// MSBBit zero if first digit is 0.
static SanitizedLiteralType sanitizeAndCountBits(std::string &IntegerLiteral) {
  llvm::erase_value(IntegerLiteral, '\''); // Skip digit separators.
  StringRef StrippedLiteral{IntegerLiteral};

  const auto MSBBit = [&StrippedLiteral](std::size_t RemainingBitCount)
                        -> std::size_t {
    char FirstDigit = StrippedLiteral.front();

    if (FirstDigit == '0') {
      return 0;
    } else if (FirstDigit <= '1') {
      return RemainingBitCount + 1;
    } else if (FirstDigit <= '3') {
      return RemainingBitCount + 2;
    } else if (FirstDigit <= '7') {
      return RemainingBitCount + 3;
    } else {
      return RemainingBitCount + 4;
    }
  };

  if(StrippedLiteral.consume_front("0b"))
  {
    StrippedLiteral = StrippedLiteral.take_while(
      [](char c){return c == '0' || c == '1'; });
    assert(!StrippedLiteral.empty());

    return { StrippedLiteral,
        MSBBit(StrippedLiteral.size() - 1), 
        StrippedLiteral.size(), 
        2 };
  }
  else if(StrippedLiteral.consume_front("0x"))
  {
    StrippedLiteral = StrippedLiteral.take_while(llvm::isHexDigit);
    assert(!StrippedLiteral.empty());

    return { StrippedLiteral,
      MSBBit((StrippedLiteral.size() - 1) * 4),
      StrippedLiteral.size() * 4,
      16 };
  }
  else if(StrippedLiteral != "0" && StrippedLiteral.consume_front("0"))
  {
    StrippedLiteral = StrippedLiteral.take_while(
      [](char c){ return c >= '0' || c <= '7'; });
    assert(!StrippedLiteral.empty());

    return { StrippedLiteral,
        MSBBit((StrippedLiteral.size() - 1) * 3),
        StrippedLiteral.size() * 3,
        8 };
  } else {
    return { StrippedLiteral, 0, 0, 10 };
  }
}

void IntegerConstantCheck::registerMatchers(MatchFinder *Finder) {
  Finder->addMatcher(integerLiteral().bind("integer"), this);
}

void IntegerConstantCheck::check(
    const MatchFinder::MatchResult &Result) {
  const auto *MatchedInt = Result.Nodes.getNodeAs<IntegerLiteral>("integer");

  assert(MatchedInt);

  QualType IntegerLiteralType = MatchedInt->getType();
  uint64_t LiteralBitWidth = Result.Context->getTypeSize( IntegerLiteralType );

  llvm::APInt LiteralValue = MatchedInt->getValue();

  std::string LiteralStr =
      Lexer::getSourceText(
          CharSourceRange::getTokenRange(MatchedInt->getSourceRange()),
          *Result.SourceManager, Result.Context->getLangOpts(), nullptr)
          .lower();

  // FIXME: There are two problematic cases where we cannot read the character.
  // With macros, in some cases (such as when not passing an argument) the
  // integer literal's token range will be 0 long.
  if (LiteralStr.empty())
    return;
  // A template function with an integer literal template argument will warn in
  // both the argument, and the function body. In the instantiated body, the
  // source range will contain the argument name, not the literal.
  // FIXME: This disables checking macro literals entirely.
  if (!llvm::isDigit(LiteralStr[0]))
    return;

  const SanitizedLiteralType SanitizedLiteral = sanitizeAndCountBits(LiteralStr);

  // Only potential edge case is "0", handled by sanitizeAndCountBits.
  assert(!SanitizedLiteral.StrippedLiteral.empty() &&
         "integer literal should not be empty");

  assert(SanitizedLiteral.MSBBit <= LiteralBitWidth &&
         "integer literal has more bits set than its bit width");

  bool IsMax = LiteralValue.isMaxValue() || LiteralValue.isMaxSignedValue();
  bool IsUnsignedMaxMinusOne = (LiteralValue + 1).isMaxValue();
  bool IsMin = LiteralValue.isMinValue() || LiteralValue.isMinSignedValue();
  bool RepresentsZero = LiteralValue.isNullValue();

  bool IsMSBBitUsed = SanitizedLiteral.MSBBit == LiteralBitWidth;
  // Can be greater, eg. an 8-bit UCHAR_MAX byte value represented by 377 octal
  bool IsMSBByteUsed = SanitizedLiteral.MSBByte >= LiteralBitWidth;
  bool HasLeadingZeroes = SanitizedLiteral.StrippedLiteral[0] == '0';

  if (IsMax || IsUnsignedMaxMinusOne) {
    diag(MatchedInt->getBeginLoc(),
         "non-portable integer literal: hardcoded platform-specific maximum value");
  } else if (IsMin && !RepresentsZero) {
    diag(MatchedInt->getBeginLoc(),
         "non-portable integer literal: hardcoded platform-specific minimum value");
  } else if (HasLeadingZeroes && !RepresentsZero) {
    diag(MatchedInt->getBeginLoc(),
         "non-portable integer literal: integer literal with leading zeroes");
  // Matches only the most significant bit,
  // eg. unsigned value 0x80000000.
  } else if (IsMSBBitUsed) {
    diag(MatchedInt->getBeginLoc(),
         "non-portable integer literal: should not rely on the most significant bit");
  // Matches the most significant byte,
  // eg. literals like 0x30000000.
  } else if (IsMSBByteUsed) {
    diag(MatchedInt->getBeginLoc(),
         "non-portable integer literal: should not rely on bits of most significant byte");
  }
}

} // namespace clang::tidy::portability
