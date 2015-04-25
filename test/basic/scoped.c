int main() {
    int x = 0;
    int z = 3;
    int z1 = 3;
    x += z + z1;
    {
        int z = 4;
        x += z;
        {
            int z = 5;
            x += z;
        }
    }
    {
        int z = 1;
        x += z;
    }

    return x;
}
