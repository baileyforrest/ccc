//test return 5
// Test nested local struct
struct foo {
    int x;
    int y;

    struct yolo {
        int a;
        int b;
    } swag;
};

int __test() {
    int x = 4;
    struct foo bar = { 1, 2, { 3, x } };

    return bar.x + bar.swag.b;
}
