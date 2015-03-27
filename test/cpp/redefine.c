// Should have no errors
#define FOUR (2 + 2)
#define FOUR         (2    +    2)
#define FOUR (2 /* two */ + 2)

// Should report errors
#define FIVE (2 + 2)
#define FIVE ( 2+2 )
#define FIVE (2 * 2)
#define FIVE(score,and,seven,years,ago) (2 + 2)

int main() {
    return 0;
}
