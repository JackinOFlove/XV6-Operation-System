#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int pipeAry[12][2];
void prime(int layer)
{
    if (layer == 11)
    {
        exit(0);
    }
    int *EOF = (int *)malloc(sizeof(int));
    *EOF = -1;
    int *num = (int *)malloc(sizeof(int)); // 存放读出来的数
    int min_num = 0;
    read(pipeAry[layer][0], num, sizeof(int)); // 读数据
    if (*num == *EOF)
    {
        fprintf(1, "over!");
        exit(0);
    }
    else
    {
        min_num = *num;
        fprintf(1, "prime %d\n", min_num);
        if (pipe(pipeAry[layer + 1]) == -1)
        {
            // 若创建右侧管道失败
            exit(2);
        }
    }
    int pid = fork();
    if (pid < 0)
    {
        exit(3);
    }
    if (pid == 0)
    {
        close(pipeAry[layer + 1][1]);
        prime(layer + 1);
        return;
    }
    else
    {
        close(pipeAry[layer][1]);
        close(pipeAry[layer + 1][0]);
        while (read(pipeAry[layer][0], num, sizeof(int)) && *num != *EOF)
        {
            // fprintf(1,"Layer: %d num:%d\n",layer,min_num);
            if ((*num) % min_num != 0)
            {
                // 加入右侧管道
                write(pipeAry[layer + 1][1], num, sizeof(int));
            }
        }
        write(pipeAry[layer + 1][1], EOF, sizeof(int));
        close(pipeAry[layer + 1][1]);
        close(pipeAry[layer][0]);
        wait(0);
        free(num);
    }
    free(EOF);
}
int main(int argc, char *argv[])
{
    if (argc != 1)
    {
        exit(1);
    }
    if (pipe(pipeAry[0]) == -1)
    {
        exit(2);
    }
    int pid = fork();
    if (pid < 0)
    {
        exit(3);
    }
    int *EOF = (int *)malloc(sizeof(int));
    *EOF = -1;
    if (pid == 0)
    {
        close(pipeAry[0][1]);
        prime(0);
        close(pipeAry[1][1]);
    }
    else
    {
        close(pipeAry[0][0]);
        int status = 0;
        // 放入2-35
        for (int i = 2; i <= 35; ++i)
        {
            write(pipeAry[0][1], &i, sizeof(i));
        }
        write(pipeAry[0][1], EOF, sizeof(int));
        close(pipeAry[0][1]);
        wait(&status);
    }
    free(EOF);
    exit(0);
}