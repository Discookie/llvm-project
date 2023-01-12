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

  std::string LiteralStr =
      Lexer::getSourceText(
          CharSourceRange::getTokenRange(MatchedInt->getSourceRange()),
          *Result.SourceManager, Result.Context->getLangOpts(), nullptr)
          .lower();

  // FIXME: In some cases, a macro expansion happens where we cannot parse the source text.
  if (LiteralStr.size() == 0)
    return;

  llvm::APInt LiteralValue = MatchedInt->getValue();

  auto SanitizedLiteral =
      sanitizeAndCountBits(LiteralStr);
  QualType IntegerLiteralType = MatchedInt->getType();
  unsigned int LiteralBitWidth = Result.Context->getTypeSize( IntegerLiteralType );

  /*
  // for testing purpose
  llvm::errs() << "--------------------------------" << "\n\n"
              << "\n LiteralStr: " << LiteralStr << "\n APINT: " << LiteralValue
               << "  StrippedLiteral:" << SanitizedLiteral.StrippedLiteral
               << "  str size:" << SanitizedLiteral.StrippedLiteral.size() << "\n\n"
               << "  Type: " << IntegerLiteralType.getAsString() << "\n\n"
               << "  Size: " << LiteralBitWidth << "\n\n"
               << "  MSB: " << SanitizedLiteral.MSBBit << "\n\n"
               << "  Radix: " << SanitizedLiteral.Radix << "\n\n"
               << "  max: " << llvm::APInt::getMaxValue(LiteralBitWidth ) << "\n\n"
               << "  max signed: " << llvm::APInt::getSignedMaxValue(LiteralBitWidth ) << "\n\n"
               << "  IsMax: " << LiteralValue.isMaxValue() << "\n\n"
               << "  IsMaxSigned: " << LiteralValue.isMaxSignedValue() << "\n\n"
               << "  IsMax -1 : " << (MatchedInt->getValue()+1).isMaxValue() << "\n\n"
               << "  IsMaxSigned -1 : " << (MatchedInt->getValue()+1).isMaxSignedValue() << "\n\n"
               << "  IsMin: " << LiteralValue.isMinValue() << "\n\n"
               << "  IsMinSigned: " << LiteralValue.isMinSignedValue() << "\n\n"
               << "--------------------------------" << "\n\n";
  */

  // Only potential edge case is "0", handled by sanitizeAndCountBits.
  assert(!SanitizedLiteral.StrippedLiteral.empty() && "integer literal should not be empty");

  assert(SanitizedLiteral.MSBBit <= LiteralBitWidth && "integer literal has more bits set than its bit width");
  const bool IsFullPattern = SanitizedLiteral.MSBBit == LiteralBitWidth;
  // Can be greater, eg. an 8-bit BYTE_MAX byte value represented by 377 octal
  const bool IsFullPatternAlternate = SanitizedLiteral.MSBByte >= LiteralBitWidth;
  const bool RepresentsZero = LiteralValue.isNullValue();
  const bool HasLeadingZeroes = SanitizedLiteral.StrippedLiteral[0] == '0';
  const bool IsMax = LiteralValue.isMaxValue() || LiteralValue.isMaxSignedValue();
  const bool IsMin = LiteralValue.isMinValue() || LiteralValue.isMinSignedValue();
  const bool IsUnsignedMaxMinusOne = (LiteralValue + 1).isMaxValue();

  const bool IntegralPattern =
      (HasLeadingZeroes && !RepresentsZero) || 
      IsMax || IsUnsignedMaxMinusOne ||
      (IsMin && !RepresentsZero) ||
      IsFullPattern || IsFullPatternAlternate;

  if (IntegralPattern) {
    diag(MatchedInt->getBeginLoc(),
         "integer is being used in a non-portable manner");

    if (HasLeadingZeroes && !RepresentsZero) {
      diag(MatchedInt->getBeginLoc(),
           "integer literal has leading zeroes", DiagnosticIDs::Note);
    } else if (IsMax || IsUnsignedMaxMinusOne) {
      diag(MatchedInt->getBeginLoc(),
           "do not hardcode integer maximum value", DiagnosticIDs::Note);
    } else if (IsMin && !RepresentsZero) {
      diag(MatchedInt->getBeginLoc(),
           "do not hardcode integer minimum value", DiagnosticIDs::Note);
    // Matches only the most significant bit,
    // eg. unsigned value 0x80000000, but not 0x30000000.
    } else if (IsFullPattern) {
      diag(MatchedInt->getBeginLoc(),
           "error-prone literal: should not rely on the most significant bit",
           DiagnosticIDs::Note);
    // This warning also matches 0x30000000, for statistics purposes for now.
    } else if (IsFullPatternAlternate) {
      diag(MatchedInt->getBeginLoc(),
           "error-prone literal: should not rely on the most significant bit (alternate)",
           DiagnosticIDs::Note);
    }
  }
}

} // namespace portability
} // namespace tidy
} // namespace clang
