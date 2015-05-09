//test return 1519848
int __test() {
    int sum = 0;
    for (int x = 0; x < 10000; ++x) {
        if (x % 33 != 0) {
            continue;
        }
        sum += x;
    }

    return sum;
}
