#include <stdio.h>

#define STRIFY(foo) #foo
#define STR "foo"

int main() {
    printf("%d %s\n", __LINE__, __FILE__ __DATE__ __TIME__);
    printf("%s\n", "hello"STR STRIFY(bob));
    return 0;
}
