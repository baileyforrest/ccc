//test return 88

struct foo1 {
    int main;
    void *yolo;
    char c;
    double x;
};

union bar1 {
    int main;
    void *yolo;
    char c;
    double x;
};

struct foo2 {
    char hello[7];
    struct foo1 *swag;
    char a;
    long x;
    int y;
    short g;
};

union bar2 {
    char hello[7];
    struct foo1 *swag;
    char a;
    long x;
    int y;
    short g;
};


int __test() {
    struct foo1 *hello;
    union bar2 there;
    return sizeof(struct foo2) + sizeof(*hello) + sizeof(union bar1) +
        sizeof(there);
}
