//test return -200
typedef union foo {
    long x;
    char arr[sizeof(long) / 2];
} foo;

union foo bar = { .arr = { 0xde, 0xad, 0xbe, 0xef } };

int __test() {
    int res = 0;
    for (int i = 0; i < sizeof(bar.arr); ++i) {
        res += bar.arr[i];
    }

    return res;
}
