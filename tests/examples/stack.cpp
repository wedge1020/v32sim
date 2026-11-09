#define BUMP(v)  v = v + 1
int total;
struct Acc {
    int sum;
    int add(int a, int b)
    {
        int s = a + b;
        BUMP(s);
        sum = sum + s;
        return s;
    }
};
int main(void)
{
    Acc acc;
    acc.sum = 0;
    int r = acc.add(3, 4);
    int dead = 0;
    int i;
    for (i = 0; i < 4; i++) { dead = dead + i; r = r + i; }
    total = r;
    return 0;
}
