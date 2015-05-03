//test return 4
// Tests array initializer as local variable

int __test() {
    int x = 3;
    int arr[] = { 1, 2, 3, 4, 5, x };

    return arr[3];
}
