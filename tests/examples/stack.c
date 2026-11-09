#define BUMP(v)  v = v + 1
int total;
int add(int a, int b)
{
    int sum = a + b;
    BUMP(sum);
    total = total + sum;
    return sum;
}
int main(void)
{
    int r = add(3, 4);
    int dead = 0;
    int i;
    for (i = 0; i < 4; i++) { dead = dead + i; r = r + i; }
    total = r;
    return 0;
}
