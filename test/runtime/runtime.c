#include <stdio.h>

extern int __test();

int main() {
    printf("%d\n", __test());
    return 0;
}
