#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/param.h"

int main(int argc, char *argv[])
{
    char *args[MAXARG]; // 保存执行的参数
    int p;
    for (p = 0; p < argc; p++) // 先把xargs自带的参数读进去
        args[p] = argv[p];
    char buf[256];

    while (1) // 进入循环，每次读一行内容
    {
        int i = 0;
        while ((read(0, buf + i, sizeof(char)) != 0) && buf[i] != '\n') // 读取标准输入一行的内容
            i++;
        if (i == 0) // 读完所有行
            break;
        buf[i] = 0;      // 字符串结尾，exec要求的
        args[p] = buf;   // 把标准输入传进的一行参数附加到xargs这个函数后面
        args[p + 1] = 0; // exec读到0就表示读完了
        if (fork() == 0)
        {                            // 子进程
            exec(args[1], args + 1); // 前者是xargs，后者是参数，执行成功会自动退出
            printf("exec err\n");    // 如果执行失败，会打印以下信息
        }
        else
            wait((void *)0);
    }
    exit(0);
}