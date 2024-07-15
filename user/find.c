#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

// strcmp return 0 if two char* is equal !!

void find(char *path, char *filename)
{
    char buf[512], *p;
    int fd;
    struct dirent de;
    struct stat st;

    if ((fd = open(path, 0)) < 0)
    {
        fprintf(2, "find: cannot open %s\n", path);
        return;
    }

    if (fstat(fd, &st) < 0)
    {
        fprintf(2, "find: cannot stat %s\n", path);
        close(fd);
        return;
    }

    if (st.type == T_FILE)
    { // it should be a dir
        fprintf(2, "find: can't find files in a file\n");
        exit(1);
    }

    // T_DIR:
    if (strlen(path) + 1 + DIRSIZ + 1 > sizeof buf)
    { // too long ..
        printf("find: path too long\n");
        exit(1);
    }

    strcpy(buf, path);

    p = buf + strlen(buf);
    *p++ = '/'; // p is pointed to the last file/dir name

    while (read(fd, &de, sizeof(de)) == sizeof(de))
    {
        if (de.inum == 0)
            continue;
        memmove(p, de.name, DIRSIZ);
        p[DIRSIZ] = 0;
        if (stat(buf, &st) < 0)
        {
            fprintf(2, "find: cannot stat %s\n", buf);
            continue;
        }
        // return 0 if s1 equals to s2 !!
        if (st.type == T_DIR && strcmp(".", p) != 0 && strcmp("..", p) != 0)
            find(buf, filename); // recursion
        else if (st.type == T_FILE && strcmp(p, filename) == 0)
            printf("%s\n", buf); // print the output
    }

    close(fd);
}

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        fprintf(2, "too little arguments..\n");
        exit(1);
    }
    find(argv[1], argv[2]);
    exit(0);
}