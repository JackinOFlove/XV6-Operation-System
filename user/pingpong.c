#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char *argv[])
{
    if (argc != 1) // 首先检查命令行参数个数，若不为1直接输出error
    {
        fprintf(2, "Error...\n");
        exit(1);
    }
    int p[2]; // 定义一个整型数组 p，用于存放管道的两个文件描述符
    pipe(p);  // 创建管道，p[0] 为读端，p[1] 为写端

    if (fork() == 0) // 如果fork() == 0，则为子进程
    {
        close(p[0]); // 子进程是需要写的，使用关闭读端

        char temp = 'x';
        if (write(p[1], &temp, 1))
            fprintf(0, "%d: received ping\n", getpid()); // 向标准输出打印消息，包含子进程的 PID

        close(p[1]); // 子进程写完了关闭写端
    }
    else // 此时为父进程
    {
        wait((int *)0); // 父进程需要等待子进程结束
        close(p[1]);    // 父进程读，关闭写端

        char temp;
        if (read(p[0], &temp, 1))
            fprintf(0, "%d: received pong\n", getpid()); // 向标准输出打印消息，包含父进程的 PID

        close(p[0]); // 父进程读完，关闭读端
    }
    exit(0);
}