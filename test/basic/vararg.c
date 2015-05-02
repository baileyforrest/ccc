//test return 8
// Tests calling vaarg function
#include <stdio.h>

int __test() {
    return printf("%d %d %s\n", 1, 2, "foo");
}
