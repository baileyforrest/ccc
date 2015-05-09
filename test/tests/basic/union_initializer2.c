//test return -272716322
typedef union foo {
    int x;
    char arr[sizeof(int)];
} foo;

int __test() {
    union foo bar = { .arr = { 0xde, 0xad, 0xbe, 0xef } };

    return bar.x;
}
