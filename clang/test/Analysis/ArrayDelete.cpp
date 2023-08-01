// RUN: %clang_cc1 -analyze -analyzer-checker=alpha.cplusplus.ArrayDelete -std=c++11 -verify -analyzer-output=text %s

struct Base {
    virtual ~Base() = default;
};

struct Derived : public Base {};

struct DoubleDerived : public Derived {};

Derived *get();

Base *create() {
    Base *b = new Derived[3];
}

void sink(Base *b) {
    delete[] b;
}

void sink_cast(Base *b) {
    delete[] reinterpret_cast<Derived*>(b);
}

void sink_derived(Derived *d) {
    delete[] d;
}



void same_function() {
    Base *sd = new Derived[10];
    delete[] sd;
    
    Base *dd = new DoubleDerived[10];
    delete[] dd;
}

void different_function() {
    Base *assigned = get();
    delete[] assigned;

    Base *indirect;
    indirect = get();
    delete[] indirect;

    Base *created = create();
    delete[] created;

    Base *sb = new Derived[10];
    sink(sb);
}

void safe_function() {
    Base *sb = new Derived[10];
    sink_cast(sb);

    Derived *sd = new Derived[10];
    sink_derived(sd);
}
