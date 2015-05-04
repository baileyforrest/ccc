//test return 5
// Test anonymous struct
struct foo {
    int x;
    int y;
    struct {
        int a;
        int b;
    };
};

int __test() {
    int x = 4;
    struct foo bar = { 1, 2, { 3, x } };

    return bar.x + bar.b;
}
