//test error
int __test() {
    _Static_assert(0, "foo");

    return 0;
}
