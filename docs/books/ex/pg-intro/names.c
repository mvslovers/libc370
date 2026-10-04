/* Two functions whose names agree in the first eight
   characters: give each an external name of its own. */
int process_input(void)  asm("PRCINPUT");
int process_output(void) asm("PRCOUTPT");

int process_input(void)
{
    return 1;
}

int process_output(void)
{
    return 2;
}
