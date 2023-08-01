struct A {
  int i;
  virtual ~A() = default;
};

struct B : A {
  int j;
};

void deleter(A *a) {
  delete[] a;
}

B *creater() {
  return new B[3];
}

void standalone_deleter(B *b) {
  A *a = (A*)b;
  delete[] a;
}

int main() {
  B *b = new B[3];
  A *a = (A*) b;

  delete[] a;
  deleter(creater());

  return 0;
}


