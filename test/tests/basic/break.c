//test return 3333333
int __test() {
    int iters = 0;
    for (int x = 0, y = 4; ; ++x, y += 2, ++iters) {
        if (x + y > 10000000) {
            break;
        }
    }

    return iters;
}
