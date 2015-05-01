//test return 3
// Tests struct initializers, member accesses

struct foo {
    int x;
    int y;
};

struct foo z = { 1, 2 };
struct foo *a = &z;

int __test() {
    return z.x + a->y;
}
