//test return 7
// Tests handling of local and global variables

int global = 3;

int __test() {
    int local = 4;
    return global + local;
}
