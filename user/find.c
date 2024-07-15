#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

char *pfilename(char *path)
{
    char *p;
    for (p = path + strlen(path); p >= path && *p != '/'; p--) // 找到最后一个斜杠后的第一个字符.
        ;
    p++;
    return p;
}

int find(char *path, char *filename)
{
    int fd;            // 存储打开的目录的文件描述符
    char buf[512], *p; // buf用于存储当前正在检查的目录或文件的路径,p用作遍历和操作buf数组
    struct stat st;    // 用于存储文件或目录的信息
    struct dirent de;  // 表示一个目录条目，用于遍历目录的内容

    if ((fd = open(path, 0)) < 0) // 如果open系统调用失败（返回负值），则会打印错误消息并退出
    {
        fprintf(2, "open fail%s\n", path);
        exit(1);
    }
    if (fstat(fd, &st) < 0) // 获取有关打开的文件或目录的信息
    {
        fprintf(2, "fstat fail%s\n", path); // 失败就打印错误消息，关闭文件描述符，然后退出
        close(fd);
        exit(1);
    }

    switch (st.type)
    {
    case T_FILE:                                    // 常规文件
        if (0 == strcmp(pfilename(path), filename)) // 比较文件名与指定的文件名,如果匹配就打印
            fprintf(1, "%s\n", path);               // 标准输出
        break;
    case T_DIR:                // 目录
        strcpy(buf, path);     // 将给定路径复制到buf中
        p = buf + strlen(buf); // 将指针移动到buf的末尾
        *p++ = '/';
        while (read(fd, &de, sizeof(de)) == sizeof(de))
        {                     // 读取目录的内容,将结果存储在de结构体中
            if (de.inum == 0) // 如果读取的条目的inum字段为0，表示无效条目，代码将继续下一次循环
                continue;
            // 如果读取的条目的名称是当前目录（.）或上级目录（..），代码也将继续下一次循环
            if (0 == strcmp(".", de.name) || 0 == strcmp("..", de.name))
                continue;
            // 拼接路径，将条目的名称复制到buf中。并通过调用stat函数获取该条目的信息存储在 st 结构体中。
            memmove(p, de.name, DIRSIZ);
            p[DIRSIZ] = 0; // 在buf数组中的目录项名称的末尾添加一个空字符（\0），以确保字符串以空字符结尾，从而使其成为一个有效的C字符串
            if (stat(buf, &st) < 0)
            {
                printf("find: cannot stat %s\n", buf);
                continue;
            }
            find(buf, filename); // 递归
        }
        break;
    }
    close(fd); // 关闭文件描述符，并返回0表示函数执行成功结束
    return 0;
}

int main(int argc, char *argv[])
{
    if (argc < 3)
    {
        fprintf(2, "not enough arguments\n");
        exit(1);
    }
    find(argv[1], argv[2]);
    exit(0);
}