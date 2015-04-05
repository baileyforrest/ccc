/**
 * Make sure trailing space is trimmed from macro parameters
 */

#define STR(x) #x

int main() {
    return sizeof(STR(foo     ));
}
