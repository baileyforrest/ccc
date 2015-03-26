int main(int argc) {
    const int *const *volatile *const *const a;
    int b[4][5][6];
    int (***c)(int z)[5];
    int *(*const *volatile (* d))(int z);
    return 0;
}
