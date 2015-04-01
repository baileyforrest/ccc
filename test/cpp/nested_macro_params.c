/**
 * Check paramaters in nested macros
 */
#define FOO(a, b) ((a) + (b))
#define BAR(c, d) FOO(c*d, c*d)
int main() {
    return BAR(1, 2);
}
