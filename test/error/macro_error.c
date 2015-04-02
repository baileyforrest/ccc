/**
 * Tests proper error reporting inside of macros.
 */
#include "macro_error.h"

int main() {
    int y;
    FOO(y);
    return 0;
}
