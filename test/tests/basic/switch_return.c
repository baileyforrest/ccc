//test return 1337

int __test() {
    int x = 3;

    switch (x) {
    case 1: return 4;
    case 2: return 5;

    case 17: return 6;
    case 5: return 7;
    default: return 1337;
    }
}
