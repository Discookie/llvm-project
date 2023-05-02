// RUN: %check_clang_tidy %s -std=c++17-or-later portability-integer-constant %t

using int32_t = decltype(42);

void regular() {
  // no-warnings
  0;
  00;
  0x0;
  0x00;
  0b0;
  0b00;
  0b0'0'0;

  -1;
  -0X1;

  127;
  0x7'f;

  -128;
  -0x80;

  256;
  0X100;

  42;
  0x2A;

  180079837;
  0xabbccdd;

  // FIXME: (only 31 bits) False positive, reported as max signed int.
  0b1111111111111111111111111111111;
  // CHECK-MESSAGES: :[[@LINE-1]]:3: warning: non-portable integer literal: hardcoded platform-specific maximum value [portability-integer-constant]

  // FIXME: Large numbers represented as hex are the most common false positive,
  // eg. the following literal is a 64-bit prime.
  0xff51afd7ed558ccd;
  // CHECK-MESSAGES: :[[@LINE-1]]:3: warning: non-portable integer literal: should not rely on the most significant bit [portability-integer-constant]

  // FIXME: Fixed-size integer literals are a common false positive as well.
  int32_t literal = 0x40000000;
  // CHECK-MESSAGES: :[[@LINE-1]]:21: warning: non-portable integer literal: should not rely on bits of most significant byte [portability-integer-constant]

  // FIXME: For arrays of many integer constants, every single one is reported, leading to a large number of reports in cryptography code.
  unsigned int array[] = { 0xff51afd7, 0xed558ccd };
  // CHECK-MESSAGES: :[[@LINE-1]]:28: warning: non-portable integer literal: should not rely on the most significant bit [portability-integer-constant]
  // CHECK-MESSAGES: :[[@LINE-2]]:40: warning: non-portable integer literal: should not rely on the most significant bit [portability-integer-constant]

  // FIXME: According to the standard, the type of the integer literal is the
  // smallest type it can fit into. While technically a false positive, it could
  // signal literal width confusion regardless.
  long long int long_literal = 0x7fffffff;
  // CHECK-MESSAGES: :[[@LINE-1]]:32: warning: non-portable integer literal: hardcoded platform-specific maximum value [portability-integer-constant]
}

// FIXME: Enums values are also reported individually.
enum EnumValues {
  One = 0x01,
  // CHECK-MESSAGES: :[[@LINE-1]]:9: warning: non-portable integer literal: integer literal with leading zeroes [portability-integer-constant]
  Two = 0x02
  // CHECK-MESSAGES: :[[@LINE-1]]:9: warning: non-portable integer literal: integer literal with leading zeroes [portability-integer-constant]
};

// INT_MIN, INT_MAX, UINT_MAX, UINT_MAX-1
// All binary literals are 32 bits long
void limits_int() {
  // FIXME: not recognize as Min
  -214748'3'648;
  // --CHECK-MESSAGES: :[[@LINE-1]]:4: warning: non-portable integer literal: hardcoded platform-specific minimum value [portability-integer-constant]
  // --CHECK-MESSAGES: :[[@LINE-1]]:4: warning: non-portable integer literal: hardcoded platform-specific minimum value [portability-integer-constant]

  -0x80'00'00'00;
  // CHECK-MESSAGES: :[[@LINE-1]]:4: warning: non-portable integer literal: hardcoded platform-specific minimum value [portability-integer-constant]

  -020000000000;
  // CHECK-MESSAGES: :[[@LINE-1]]:4: warning: non-portable integer literal: hardcoded platform-specific minimum value [portability-integer-constant]

  -0b10000000000000000000000000000000;
  // CHECK-MESSAGES: :[[@LINE-1]]:4: warning: non-portable integer literal: hardcoded platform-specific minimum value [portability-integer-constant]



  21474'83647;
  // CHECK-MESSAGES: :[[@LINE-1]]:3: warning: non-portable integer literal: hardcoded platform-specific maximum value [portability-integer-constant]

  0x7FFFFFFF;
  // CHECK-MESSAGES: :[[@LINE-1]]:3: warning: non-portable integer literal: hardcoded platform-specific maximum value [portability-integer-constant]

  017777777777;
  // CHECK-MESSAGES: :[[@LINE-1]]:3: warning: non-portable integer literal: hardcoded platform-specific maximum value [portability-integer-constant]

  429'4'96'7295u;
  // CHECK-MESSAGES: :[[@LINE-1]]:3: warning: non-portable integer literal: hardcoded platform-specific maximum value [portability-integer-constant]

  0xFFFFFFFF;
  // CHECK-MESSAGES: :[[@LINE-1]]:3: warning: non-portable integer literal: hardcoded platform-specific maximum value [portability-integer-constant]

  037777777777;
  // CHECK-MESSAGES: :[[@LINE-1]]:3: warning: non-portable integer literal: hardcoded platform-specific maximum value [portability-integer-constant]

  0B11111111111111111111111111111111;
  // CHECK-MESSAGES: :[[@LINE-1]]:3: warning: non-portable integer literal: hardcoded platform-specific maximum value [portability-integer-constant]

  4294967294;  // recognized as long ( Not UINT_MAX-1)
  // --CHECK-MESSAGES: :[[@LINE-1]]:3: warning: non-portable integer literal: hardcoded platform-specific maximum value [portability-integer-constant]

  0XFFFFFFFE;
  // CHECK-MESSAGES: :[[@LINE-1]]:3: warning: non-portable integer literal: hardcoded platform-specific maximum value [portability-integer-constant]

  037777777776;
  // CHECK-MESSAGES: :[[@LINE-1]]:3: warning: non-portable integer literal: hardcoded platform-specific maximum value [portability-integer-constant]

  0B11111111111111111111111111111110;
  // CHECK-MESSAGES: :[[@LINE-1]]:3: warning: non-portable integer literal: hardcoded platform-specific maximum value [portability-integer-constant]
}

// LLONG_MIN, LLONG_MAX, ULLONG_MAX, ULLONG_MAX-1
// Most binary literals are 64 bits long
void limits_llong() {
  -9'22337203'6854775808LL;
  // CHECK-MESSAGES: :[[@LINE-1]]:4: warning: non-portable integer literal: hardcoded platform-specific minimum value [portability-integer-constant]

  0x8000000000000000;
  // CHECK-MESSAGES: :[[@LINE-1]]:3: warning: non-portable integer literal: hardcoded platform-specific minimum value [portability-integer-constant]

  01000000000000000000000;
  // CHECK-MESSAGES: :[[@LINE-1]]:3: warning: non-portable integer literal: hardcoded platform-specific minimum value [portability-integer-constant]

  0b1000000000000000000000000000000000000000000000000000000000000000;
  // CHECK-MESSAGES: :[[@LINE-1]]:3: warning: non-portable integer literal: hardcoded platform-specific minimum value [portability-integer-constant]

  9223372036854775807;
  // CHECK-MESSAGES: :[[@LINE-1]]:3: warning: non-portable integer literal: hardcoded platform-specific maximum value [portability-integer-constant]

  0x7FFFFFFFFFFFFFFF;
  // CHECK-MESSAGES: :[[@LINE-1]]:3: warning: non-portable integer literal: hardcoded platform-specific maximum value [portability-integer-constant]

  0777777777777777777777;
  // CHECK-MESSAGES: :[[@LINE-1]]:3: warning: non-portable integer literal: hardcoded platform-specific maximum value [portability-integer-constant]

  // 63 bits
  0b111111111111111111111111111111111111111111111111111111111111111;
  // CHECK-MESSAGES: :[[@LINE-1]]:3: warning: non-portable integer literal: hardcoded platform-specific maximum value [portability-integer-constant]

  18446744073709551615llU;
  // CHECK-MESSAGES: :[[@LINE-1]]:3: warning: non-portable integer literal: hardcoded platform-specific maximum value [portability-integer-constant]

  0xFFFFFFFFFFFFFFFF;
  // CHECK-MESSAGES: :[[@LINE-1]]:3: warning: non-portable integer literal: hardcoded platform-specific maximum value [portability-integer-constant]

  01777777777777777777777;
  // CHECK-MESSAGES: :[[@LINE-1]]:3: warning: non-portable integer literal: hardcoded platform-specific maximum value [portability-integer-constant]

  0b1111111111111111111111111111111111111111111111111111111111111111;
  // CHECK-MESSAGES: :[[@LINE-1]]:3: warning: non-portable integer literal: hardcoded platform-specific maximum value [portability-integer-constant]

  18446744073709551614llU;
  // CHECK-MESSAGES: :[[@LINE-1]]:3: warning: non-portable integer literal: hardcoded platform-specific maximum value [portability-integer-constant]

  0xFFFFFFFFFFFFFFFe;
  // CHECK-MESSAGES: :[[@LINE-1]]:3: warning: non-portable integer literal: hardcoded platform-specific maximum value [portability-integer-constant]

  01777777777777777777776;
  // CHECK-MESSAGES: :[[@LINE-1]]:3: warning: non-portable integer literal: hardcoded platform-specific maximum value [portability-integer-constant]

  0b1111111111111111111111111111111111111111111111111111111111111110;
  // CHECK-MESSAGES: :[[@LINE-1]]:3: warning: non-portable integer literal: hardcoded platform-specific maximum value [portability-integer-constant]
}

void full_patterns() {
  0x7F'FF'FF'FF;
  // CHECK-MESSAGES: :[[@LINE-1]]:3: warning: non-portable integer literal: hardcoded platform-specific maximum value [portability-integer-constant]
  -0x80'00'00'00;
  // CHECK-MESSAGES: :[[@LINE-1]]:4: warning: non-portable integer literal: hardcoded platform-specific minimum value [portability-integer-constant]
  0xFFFFFFFFu;
  // CHECK-MESSAGES: :[[@LINE-1]]:3: warning: non-portable integer literal: hardcoded platform-specific maximum value [portability-integer-constant]
  0xFF'FF'FF'Fe;
  // CHECK-MESSAGES: :[[@LINE-1]]:3: warning: non-portable integer literal: hardcoded platform-specific maximum value [portability-integer-constant]


  0x7F'FF'FFFFFFFFFFFF;
  // CHECK-MESSAGES: :[[@LINE-1]]:3: warning: non-portable integer literal: hardcoded platform-specific maximum value [portability-integer-constant]
  -0x80'0000000000000'0LL;
  // CHECK-MESSAGES: :[[@LINE-1]]:4: warning: non-portable integer literal: hardcoded platform-specific minimum value [portability-integer-constant]
  0XFFffFFFffFFFFFFFllU;
  // CHECK-MESSAGES: :[[@LINE-1]]:3: warning: non-portable integer literal: hardcoded platform-specific maximum value [portability-integer-constant]
}

void most_significant_bits() {
  0x80004000;
  // CHECK-MESSAGES: :[[@LINE-1]]:3: warning: non-portable integer literal: should not rely on the most significant bit [portability-integer-constant]

  0xAFFFFFFF;
  // CHECK-MESSAGES: :[[@LINE-1]]:3: warning: non-portable integer literal: should not rely on the most significant bit [portability-integer-constant]

  0x10000000;
  // CHECK-MESSAGES: :[[@LINE-1]]:3: warning: non-portable integer literal: should not rely on bits of most significant byte [portability-integer-constant]

  0x3000200010004000;
  // CHECK-MESSAGES: :[[@LINE-1]]:3: warning: non-portable integer literal: should not rely on bits of most significant byte [portability-integer-constant]

  0x00001000;
  // CHECK-MESSAGES: :[[@LINE-1]]:3: warning: non-portable integer literal: integer literal with leading zeroes [portability-integer-constant]

  // In some cases, an integer literal represents a smaller type than its
  // bitcount, which caused issues with MSBBit detection.
  0x0000000000000001;
  // CHECK-MESSAGES: :[[@LINE-1]]:3: warning: non-portable integer literal: integer literal with leading zeroes [portability-integer-constant]
}

template <int I> bool template_unused_reported() { return I < 0x7FFFFFFF; }
// CHECK-MESSAGES: :[[@LINE-1]]:63: warning: non-portable integer literal

template <int I> bool template_arg_not_reported() { return I < 0x7FFFFFFF; }
// CHECK-MESSAGES: :[[@LINE-1]]:64: warning: non-portable integer literal
// CHECK-MESSAGES-NOT: :[[@LINE-2]]:60: warning: non-portable integer literal

template <int I = 0x7FFFFFFF > bool template_default_reported() { return I < 0; }
// CHECK-MESSAGES: :[[@LINE-1]]:19: warning: non-portable integer literal
// CHECK-MESSAGES-NOT: :[[@LINE-2]]:74: warning: non-portable integer literal

void templates() {
  // Should be reported only at the callsite.
  template_arg_not_reported<0x7FFFFFFF>();
  // CHECK-MESSAGES: :[[@LINE-1]]:29: warning: non-portable integer literal

  template_default_reported();

  // Do not report same literal twice.
  template_default_reported<0>();
}

// FIXME: Macros are currently not distinguished from the template arguments.
void macros() {
#define FULL_LITERAL 0x0100000000000000
  FULL_LITERAL;
  // --CHECK-MESSAGES: :[[@LINE-1]]:3: warning: non-portable integer literal
  // --CHECK-MESSAGES: :[[@LINE-3]]:22: note: expanded from macro

#define PARTIAL_LITERAL(start) start##00000000
  PARTIAL_LITERAL(0x01000000);
  // --CHECK-MESSAGES: :[[@LINE-1]]:3: warning: non-portable integer literal
  // --CHECK-MESSAGES: :[[@LINE-3]]:32: note: expanded from macro

  // FIXME: In this case, the integer literal token is 0 long due to a tokenizer issue.
#define EMPTY_ARGUMENT(start) start##0x0100000000000000
  EMPTY_ARGUMENT();
  // --CHECK-MESSAGES: :[[@LINE-1]]:3: warning: non-portable integer literal
  // --CHECK-MESSAGES: :[[@LINE-3]]:38: note: expanded from macro

  // The following literals could be detected without reading the text representation.
#define MAX_VALUE_LITERAL 0x7FFFFFFF
  MAX_VALUE_LITERAL;
  // --CHECK-MESSAGES: :[[@LINE-1]]:3: warning: non-portable integer literal
  // --CHECK-MESSAGES: :[[@LINE-3]]:22: note: expanded from macro

#define MAX_VALUE_PARTIAL_LITERAL(start) start##FFFF
  MAX_VALUE_PARTIAL_LITERAL(0x7FFF);
  // --CHECK-MESSAGES: :[[@LINE-1]]:3: warning: non-portable integer literal
  // --CHECK-MESSAGES: :[[@LINE-3]]:32: note: expanded from macro

#define MAX_VALUE_EMPTY_ARGUMENT(start) start##0x7FFFFFFF
  MAX_VALUE_EMPTY_ARGUMENT();
  // --CHECK-MESSAGES: :[[@LINE-1]]:3: warning: non-portable integer literal
  // --CHECK-MESSAGES: :[[@LINE-3]]:38: note: expanded from macro
}
