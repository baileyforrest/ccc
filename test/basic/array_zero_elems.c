//test return 15
// Tests end of array is zero when array is longer than initializer

#define ARR_LEN 10

int arr[ARR_LEN] = { 1, 2, 3, 4, 5 };

int __test() {
    int count = 0;
    for (int i = 0; i < ARR_LEN; ++i) {
        count += arr[i];
    }

    return count;
}
