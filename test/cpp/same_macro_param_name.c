/**
 * Check predefined macro values
 */

#define FOO(a) a
#define BAR(a) FOO(a)

int main() {
    BAR(a);
    return 0;
}
