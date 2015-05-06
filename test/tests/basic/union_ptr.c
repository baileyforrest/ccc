//test return -4
// Test of pointers to unions basic functionality

typedef union lol {
    float f;
    char c[4];
} lol;

int __test() {
    lol bar;
    lol *foo = &bar;

    foo->f = 1.753753153e6;

    int res = 0;
    for (int i = 0; i < 4; ++i) {
        res += foo->c[i];
    }

    return res;
}
