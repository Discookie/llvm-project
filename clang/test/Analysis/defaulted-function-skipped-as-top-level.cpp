// RUN: %clang_analyze_cc1 -analyzer-checker=core -analyzer-display-progress %s 2>&1 | FileCheck %s

// Test that defaulted functions are not analyzed as top-level functions.

// CHECK: ANALYZE (Path,  Inline_Regular): {{.*}} B::B(const class A &)
// CHECK-NOT: ANALYZE (Path,  Inline_Regular): {{.*}} B::operator=(class B &&)
// CHECK-NOT: ANALYZE (Path,  Inline_Regular): {{.*}} A::operator=(class A &&)

class A {
    int a[1];
};
class B : A {
    B(const A &a) { *this = a; }
};
