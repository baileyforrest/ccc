//test return 36
// Tests basic comparison operations

int __test() {
    int x = 3, y = 4;
    int z = 0;
    if (x < y) {
        z += 1;
    }
    if (x) {
        z += 2;
    }

    return z * x * y;
}
