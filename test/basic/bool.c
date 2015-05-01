//test return 0
// Tests basic boolean operations

#include <stdbool.h>

int __test() {
    bool x = true;
    bool y = false;
    if (x && y) {
        return 1;
    } else {
        return 0;
    }
}
