//test return 57

union foo {
    int y:17, z:4;
    char c:7;
};

int __test() {
    union foo bar;
    bar.y = 1337;
    return bar.c;
}
