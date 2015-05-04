//test return 53

int __test() {
    int x = sizeof(__FILE__);
    x += __LINE__;
    x += sizeof(__DATE__);
    x += sizeof(__TIME__);
    return x;
}
