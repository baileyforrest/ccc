/**
 * Check predefined macro values
 */
#include <assert.h>

int main() {
    assert(0 && "lol");
    __x86_64__;
    return 0;
}
