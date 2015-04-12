struct foo {
    int x;
    int y;
};

struct foo z = { 1, 2 };
struct foo *a = &z;

int main() {
    return z.x + a->y;
}
