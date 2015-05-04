//test return 3833
int __test() {
    int x = 0;
#line 555
    x += sizeof(__FILE__);
    x += __LINE__;
    x += __LINE__;
#line 1337 "foo.c"
    x += sizeof(__FILE__);
    x += __LINE__;
    x += __LINE__;
    return x;
}
