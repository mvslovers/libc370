int record_count(void);
int record_check(void);

int main(void)
{
    return record_count() * 10
         + record_check();
}
