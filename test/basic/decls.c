typedef int foobar(int x);

typedef struct {
    int foo;
} cats, dogs;


int main(int argc) {
    struct nested {
        double baz;
        union bar {
            int foo;
        } roof;
    } hello, foo;
    const int *const *volatile *const *const a;
    int b[4][5][6];
    int (***c)(int z)[5];
    int *(*const *volatile (* d))(int z);
    return 0;
}
