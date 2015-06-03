//test return 15

struct foo {
    int x;
    int y:17, z:4;
    char c:7;
    long g;
};
struct foo foo = { 1, 2, 3, 4, 5 };

int __test() {
    return foo.x + foo.y + foo.z + foo.c + foo.g;
}
