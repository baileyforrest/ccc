//test return 314
#include <stddef.h>

struct foo1 {
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
    struct foo1 hi;
};

union bar2 {
    char hello[7];
    struct foo1 *swag;
    char a;
};


int __test() {
    return
        offsetof(struct foo1, main) +
        offsetof(struct foo1, yolo) +
        offsetof(struct foo1, c) +
        offsetof(struct foo1, x) +

        offsetof(struct foo2, hello) +
        offsetof(struct foo2, hello[2 + 4]) +
        offsetof(struct foo2, swag) +
        offsetof(struct foo2, a) +
        offsetof(struct foo2, x) +
        offsetof(struct foo2, y) +
        offsetof(struct foo2, g) +
        offsetof(struct foo2, hi) +
        offsetof(struct foo2, hi.main) +
        offsetof(struct foo2, hi.x) +

        offsetof(union bar2, hello) +
        offsetof(union bar2, swag) +
        offsetof(union bar2, a);
}
