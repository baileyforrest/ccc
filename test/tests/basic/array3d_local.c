//test return 34
// Tests 3d array initializer as local variable

int __test() {
    int x = 34;
    int arr[3][2][2] = { { { 1, 2 }, { 3, 4 } },
                         { { 5, 6 }, { 7, x } } };


    return arr[1][1][1];
}
