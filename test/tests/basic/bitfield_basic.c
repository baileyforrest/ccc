//test return 15

struct foo {
    int x;
    int y:17, z:4;
    char c:7;
    long g;
};

int __test() {
    struct foo foo;
    foo.x = 1;
    foo.y = 2;
    foo.z = 3;
    foo.c = 4;
    foo.g = 5;

    return foo.x + foo.y + foo.z + foo.c + foo.g;
}
