#include "kernel/types.h"
#include "user/user.h"
#include "kernel/param.h"

int main(int argc, char *argv[])
{
    int i, count = 0, k, m = 0;
    char *line_split[MAXARG], *p;
    char block[32], buf[32];
    p = buf;
    for (i = 1; i < argc; i++)
    {
        line_split[count++] = argv[i];
    }
    while ((k = read(0, block, sizeof(block))) > 0)
    {
        for (i = 0; i < k; i++)
        {
            if (block[i] == '\n')
            {
                buf[m] = 0;
                line_split[count++] = p;
                line_split[count] = 0;
                m = 0;
                p = buf;
                count = argc - 1;
                if (fork() == 0)
                {
                    exec(argv[1], line_split);
                }
                wait(0);
            }
            else if (block[i] == ' ')
            {
                buf[m++] = 0;
                line_split[count++] = p;
                p = &buf[m];
            }
            else
            {
                buf[m++] = block[i];
            }
        }
    }
    exit(0);
}