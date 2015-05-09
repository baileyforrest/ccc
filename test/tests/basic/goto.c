//test return 45

int __test() {
    int sum = 0;
    int x = 0;

loop:
    sum += x;
    ++x;
    if (x < 10) {
        goto loop;
    }

    goto here;
here:
    return sum;
}
