/**
 * Check predefined macro values
 */
typedef unsigned long size_t;

typedef struct len_str_t {
    char *str; /**< String. Possibly non null terminated */
    size_t len;      /**< Length of the string (not including NULL) */
} len_str_t;

#define LEN_STR_LIT(str) { str, sizeof(str) - 1 }

int main() {
    len_str_t hello = { "hello", sizeof("hello") - 1};
    len_str_t hello2 = { "hello", sizeof "hello" - 1};
    int x = sizeof(int);
    __x86_64__;
    return 0;
}
