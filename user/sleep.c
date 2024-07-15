#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char *argv[]) // argc:表示传递给程序的命令行参数的数量;argv:包含传递给程序的命令行参数
{
    if (argc < 2) // 检查命令行参数的数量是否小于2
    {
        fprintf(2, "Error...\n");
        exit(1);
    }
    sleep(atoi(argv[1])); // 将第1个参数转成整数,并且sleep
    exit(0);
}