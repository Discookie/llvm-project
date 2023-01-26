// RUN: %check_clang_tidy %s -std=c++17-or-later portability-non-portable-integer-constant %t

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

  // FIXME: No warnings as long as not all of the value are used
  0x3fffffff;
  // CHECK-MESSAGES: :[[@LINE-1]]:3: warning: error-prone literal

  // FIXME: (only 31 bits) shouldn't be reported, but it's recognized as MaxSigned Int
  0b1111111111111111111111111111111;
  // CHECK-MESSAGES: :[[@LINE-1]]:3: warning: error-prone literal
}

// INT_MIN, INT_MAX, UINT_MAX, UINT_MAX-1
// All binary literals are 32 bits long
void limits_int() {
  // FIXME: not recognize as Min
  -214748'3'648;
  // --CHECK-MESSAGES: :[[@LINE-1]]:4: warning: error-prone literal

  -0x80'00'00'00;
  // CHECK-MESSAGES: :[[@LINE-1]]:4: warning: error-prone literal

  -020000000000;
  // CHECK-MESSAGES: :[[@LINE-1]]:4: warning: error-prone literal

  -0b10000000000000000000000000000000;
  // CHECK-MESSAGES: :[[@LINE-1]]:4: warning: error-prone literal



  21474'83647;
  // CHECK-MESSAGES: :[[@LINE-1]]:3: warning: error-prone literal

  0x7FFFFFFF;
  // CHECK-MESSAGES: :[[@LINE-1]]:3: warning: error-prone literal

  017777777777;
  // CHECK-MESSAGES: :[[@LINE-1]]:3: warning: error-prone literal

  429'4'96'7295u;
  // CHECK-MESSAGES: :[[@LINE-1]]:3: warning: error-prone literal

  0xFFFFFFFF;
  // CHECK-MESSAGES: :[[@LINE-1]]:3: warning: error-prone literal

  037777777777;
  // CHECK-MESSAGES: :[[@LINE-1]]:3: warning: error-prone literal

  0B11111111111111111111111111111111;
  // CHECK-MESSAGES: :[[@LINE-1]]:3: warning: error-prone literal

  4294967294;  // recognized as long ( Not UINT_MAX-1)
  // --CHECK-MESSAGES: :[[@LINE-1]]:3: warning: error-prone literal

  0XFFFFFFFE;
  // CHECK-MESSAGES: :[[@LINE-1]]:3: warning: error-prone literal

  037777777776;
  // CHECK-MESSAGES: :[[@LINE-1]]:3: warning: error-prone literal

  0B11111111111111111111111111111110;
  // CHECK-MESSAGES: :[[@LINE-1]]:3: warning: error-prone literal
}

// LLONG_MIN, LLONG_MAX, ULLONG_MAX, ULLONG_MAX-1
// Most binary literals are 64 bits long
void limits_llong() {
  -9'22337203'6854775808LL;
  // CHECK-MESSAGES: :[[@LINE-1]]:4: warning: error-prone literal

  0x8000000000000000;
  // CHECK-MESSAGES: :[[@LINE-1]]:3: warning: error-prone literal

  01000000000000000000000;
  // CHECK-MESSAGES: :[[@LINE-1]]:3: warning: error-prone literal

  0b1000000000000000000000000000000000000000000000000000000000000000;
  // CHECK-MESSAGES: :[[@LINE-1]]:3: warning: error-prone literal

  9223372036854775807;
  // CHECK-MESSAGES: :[[@LINE-1]]:3: warning: error-prone literal

  0x7FFFFFFFFFFFFFFF;
  // CHECK-MESSAGES: :[[@LINE-1]]:3: warning: error-prone literal

  0777777777777777777777;
  // CHECK-MESSAGES: :[[@LINE-1]]:3: warning: error-prone literal

  // 63 bits
  0b111111111111111111111111111111111111111111111111111111111111111;
  // CHECK-MESSAGES: :[[@LINE-1]]:3: warning: error-prone literal

  18446744073709551615llU;
  // CHECK-MESSAGES: :[[@LINE-1]]:3: warning: error-prone literal

  0xFFFFFFFFFFFFFFFF;
  // CHECK-MESSAGES: :[[@LINE-1]]:3: warning: error-prone literal

  01777777777777777777777;
  // CHECK-MESSAGES: :[[@LINE-1]]:3: warning: error-prone literal

  0b1111111111111111111111111111111111111111111111111111111111111111;
  // CHECK-MESSAGES: :[[@LINE-1]]:3: warning: error-prone literal

  18446744073709551614llU;
  // CHECK-MESSAGES: :[[@LINE-1]]:3: warning: error-prone literal

  0xFFFFFFFFFFFFFFFe;
  // CHECK-MESSAGES: :[[@LINE-1]]:3: warning: error-prone literal

  01777777777777777777776;
  // CHECK-MESSAGES: :[[@LINE-1]]:3: warning: error-prone literal

  0b1111111111111111111111111111111111111111111111111111111111111110;
  // CHECK-MESSAGES: :[[@LINE-1]]:3: warning: error-prone literal
}

void full_patterns() {
  0x7F'FF'FF'FF;
  // CHECK-MESSAGES: :[[@LINE-1]]:3: warning: error-prone literal
  -0x80'00'00'00;
  // CHECK-MESSAGES: :[[@LINE-1]]:4: warning: error-prone literal
  0xFFFFFFFFu;
  // CHECK-MESSAGES: :[[@LINE-1]]:3: warning: error-prone literal
  0xFF'FF'FF'Fe;
  // CHECK-MESSAGES: :[[@LINE-1]]:3: warning: error-prone literal


  0x7F'FF'FFFFFFFFFFFF;
  // CHECK-MESSAGES: :[[@LINE-1]]:3: warning: error-prone literal
  -0x80'0000000000000'0LL;
  // CHECK-MESSAGES: :[[@LINE-1]]:4: warning: error-prone literal
  0XFFffFFFffFFFFFFFllU;
  // CHECK-MESSAGES: :[[@LINE-1]]:3: warning: error-prone literal
}

template <int I> bool template_unused_reported() { return I < 0x7FFFFFFF; }
// CHECK-MESSAGES: :[[@LINE-1]]:63: warning: error-prone literal

template <int I> bool template_arg_not_reported() { return I < 0x7FFFFFFF; }
// CHECK-MESSAGES: :[[@LINE-1]]:64: warning: error-prone literal
// CHECK-MESSAGES-NOT: :[[@LINE-2]]:60: warning: error-prone literal

template <int I = 0x7FFFFFFF > bool template_default_reported() { return I < 0; }
// CHECK-MESSAGES: :[[@LINE-1]]:19: warning: error-prone literal
// CHECK-MESSAGES-NOT: :[[@LINE-2]]:74: warning: error-prone literal

void templates() {
  // only at the callsite
  template_arg_not_reported<0x7FFFFFFF>();
  // CHECK-MESSAGES: :[[@LINE-1]]:29: warning: error-prone literal

  template_default_reported();

  // do not report twice
  template_default_reported<0>();
}

// FIXME: Macros are currently not distinguished from the template arguments.
void macros() {
#define FULL_LITERAL 0x0100000000000000
  FULL_LITERAL;
  // --CHECK-MESSAGES: :[[@LINE-1]]:3: warning: error-prone literal
  // --CHECK-MESSAGES: :[[@LINE-3]]:22: note: expanded from macro

#define PARTIAL_LITERAL(start) start##00000000
  PARTIAL_LITERAL(0x01000000);
  // --CHECK-MESSAGES: :[[@LINE-1]]:3: warning: error-prone literal
  // --CHECK-MESSAGES: :[[@LINE-3]]:32: note: expanded from macro

  // FIXME: In this case, the integer literal token is 0 long due to a tokenizer issue.
#define EMPTY_ARGUMENT(start) start##0x0100000000000000
  EMPTY_ARGUMENT();
  // --CHECK-MESSAGES: :[[@LINE-1]]:3: warning: error-prone literal
  // --CHECK-MESSAGES: :[[@LINE-3]]:38: note: expanded from macro
}
