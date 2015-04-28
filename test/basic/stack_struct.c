struct foo {
    int x;
    int y;
};

int main() {
    struct foo bar;
    bar.x = 3;

    struct foo *baz = &bar;
    baz->y = 4;

    return bar.x + bar.y;
}
