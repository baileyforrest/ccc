//test return -200
typedef union foo {
    int x;
    char arr[sizeof(int)];
} foo;

union foo bar = { 0xdeadbeef };

int __test() {
    int res = 0;
    for (int i = 0; i < sizeof(int); ++i) {
        res += bar.arr[i];
    }

    return res;
}
