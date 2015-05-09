//test return 1337

int __test() {
    int x = 3;
    int y = 0;

    switch (x) {
    case 1:
        y = 4;
        break;

    case 2:
        return 6;

    case 17:
        // FALL THROUGH
    case 5:
        y = 42;
        // FALL THROUGH
    default:
        y = 1337;
    }

    return y;
}
