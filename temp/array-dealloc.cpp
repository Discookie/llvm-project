struct Base {
    virtual ~Base() = default;
    int i;
};

struct Derived final : Base {
    int j;
};

int main() {
    Base *base_arr = new Derived[10];
    delete[] base_arr;
    return 0;
}