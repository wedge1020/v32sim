/* vars_opt.c -- V32DBG: the values `locals` shows must be true at every optimization level (v32opt rewrites ;@var loc= when
   it promotes a frame slot into a register: v32opt 20261105+; docs/repro/v32opt-promote-var.c was the failing case), and
   a `char` local is shown as `65 'A'` (type=char: v32cc 20261108+).  Break at line 14: i == 10, s == 45, c == 65. */
int g;
int leafsum(int n)
{
    int i;
    int s;
    char c;
    s = 0;
    c = 65;
    for (i = 0; i < n; i++) s += i;
    g = s + c;
    return s;
}
void main(void) { g = leafsum(10); }
