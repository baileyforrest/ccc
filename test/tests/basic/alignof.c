//test return 5
// Tests alignof operator

int __test() {
    return _Alignof(int) + _Alignof(char);
}
