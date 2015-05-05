//test return 0
typedef int foobar(int x);

typedef struct {
    int foo;
} cats, dogs;

struct foo {
    int e:4,f:5;
    int g,h,i;
    int :6;
};

struct outer {
    struct {
        int inner_elem;
    };
};

typedef int foo_t;
typedef int foo_t;

_Bool bool_test = 1;
void (*signal(int sig, void (*func)(int)))(int);

int __test(int argc) {
    struct nested {
        double baz;
        union bar {
            int foo;
        } roof;
    } hello, foo;
    const int *const *volatile *const *const a;
    int b[4][5][6];
    int *(*const *volatile (* d))(int z);
    return 0;
}
