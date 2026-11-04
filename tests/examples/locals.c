/* locals.c -- parameters, locals, an array, a struct, a pointer, a float and a shadowing block, for `locals` (V32DBG section 4).
   tests/run_harness_tests.sh builds it with v32cc -g + v32as -g (which writes locals.vbin.vars) and checks the values. */
struct pt { int x; int y; };
int g_total;
float scale(float f, int n)
{
    float r;
    r = f * n;
    return r;
}
int work(int a, int b)
{
    int arr[4];
    int i;
    int sum = 0;
    struct pt p;
    int *q;
    p.x = a; p.y = b;
    q = &sum;
    for (i = 0; i < 4; i++)
    {
        int sq = i * i;
        arr[i] = sq + a;
        sum += arr[i];
    }
    {
        int i = 99;
        sum += i;
    }
    *q += p.y;
    return sum;
}
void main(void)
{
    int r;
    float f;
    r = work(3, 5);
    f = scale(1.5, r);
    g_total = r;
}
