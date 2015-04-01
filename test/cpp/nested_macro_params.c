/**
 * Check paramaters in nested macros
 */
#define FOO(a, b) (a + b)
#define BAR(c, d) FOO(c, d)
int main() {
    BAR(1, 2);
    return 0;
}
