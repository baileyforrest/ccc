//test return 7
// Tests struct stored on stack as local variable

struct foo {
    int x;
    int y;
};

int __test() {
    struct foo bar;
    bar.x = 3;

    struct foo *baz = &bar;
    baz->y = 4;

    return bar.x + bar.y;
}
