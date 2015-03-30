/**
 * Tests concatenation in macros
 */

#define CATTER(a, b, c) a ## ## b ##\
    c ## L

int main() {
    return CATTER(1, 2, 3);
}
