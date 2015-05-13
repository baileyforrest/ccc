//test return 7

typedef struct foo {
    int x;
    int y;
} foo;

int instructer(foo bar) {
    return bar.x + bar.y;
}

int __test() {
    foo yolo = { 3, 4 };

    return instructer(yolo);
}
