struct Base {
    virtual ~Base() = default;
    int i;
};

struct Derived final : Base {
    int j;
};

struct DoubleDerived

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

int main() {
    Base *base_arr = new Derived[10];
    delete[] base_arr;
    return 0;
}
