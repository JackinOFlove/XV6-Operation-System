#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int subProcess(int *oldFd)
{
    close(oldFd[1]); // 关闭原管道写端
    int fd[2];
    int prime;
    int num;
    if (read(oldFd[0], &prime, 4)) // 若能从原管道读到数据
    {
        printf("prime %d\n", prime); // 第一个数据为质数,进行输出
        pipe(fd);                    // 创建管道和子进程
        if (fork() == 0)             // 子进程
            subProcess(fd);          // 递归调用
        else                         // 父进程
        {
            close(fd[0]);                   // 关闭新管道读端
            while (read(oldFd[0], &num, 4)) // 从原管道进行读取
            {
                if (num % prime != 0) // 不能被记录的质数整除则写入新管道
                    write(fd[1], &num, 4);
            }
            close(oldFd[0]); // 此时父进程的原管道关闭, 则关闭原管道的读端
            close(fd[1]);    // 关闭新管道的写端
            wait((int *)0);  // 等待子进程结束
        }
    }
    else
        close(oldFd[0]); // 此时说明原管道已关闭,第一个数字都读不出, 不创建子进程直接关闭原管道读端
    exit(0);
}

int main()
{
    int fd[2];
    pipe(fd);
    if (fork() == 0) // 子进程
        subProcess(fd);
    else // 父进程
    {
        close(fd[0]);
        for (int i = 2; i <= 35; ++i) // 遍历 2~35 写入管道写端
            write(fd[1], &i, 4);
        close(fd[1]); // 写完关闭管道写端并等待子进程结束
        wait((int *)0);
    }
    exit(0);
}