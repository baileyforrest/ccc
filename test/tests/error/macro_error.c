//test error
/**
 * Tests proper error reporting inside of macros.
 */
#include "macro_error.h"

int __test() {
    int y;
    FOO(y);
    return 0;
}
