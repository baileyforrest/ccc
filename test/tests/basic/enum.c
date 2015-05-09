//test return 22

typedef enum foo {
    FOO,
    BAR,
    BAZ = 10,
    GOO,
} foo;

int __test() {
    return FOO + BAR + BAZ + GOO;
}
