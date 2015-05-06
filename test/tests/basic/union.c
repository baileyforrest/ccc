//test return -4
// Test of basic union functionality

int __test() {
    union lol {
        float f;
        char c[4];
    } foo;

    foo.f = 1.753753153e6;

    int res = 0;
    for (int i = 0; i < 4; ++i) {
        res += foo.c[i];
    }

    return res;
}
