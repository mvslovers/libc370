/*
 * tstcrtri.c - libc370 #546: the inner module tstcrtrs.c LINKs.
 *
 * A C program that does nothing: @@CRT0 builds its runtime (PPA, CRT,
 * GRT), main() returns, @@EXITA tears it down.  Whatever that cycle
 * leaves behind, tstcrtrs counts.  Linked twice: TSTCRTRI against this
 * tree's libc.a (green), TSTCRTRR against the installed one (red).
 */
int main(void)
{
    return 0;
}
