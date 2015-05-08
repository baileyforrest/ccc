//test return 4
/**
 * Tests designated struct initialzers
 */

struct foo {
    struct {
        int x;
        int y;
    } a;
    struct {
        int i;
        int j;
    } b;
};

int __test() {
    struct foo lol = { .b = { .j = 3, .i = 4 }, .a = { .y = 1, .x = 2 } };

    return lol.b.i;
}
