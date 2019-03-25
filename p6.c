#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <errno.h>

int main(int argc, char *argv[])
{
    DIR *dirp;
    struct dirent *direntp;
    struct stat stat_buf;
    char *str;
    char fname[258];

    if (argc != 2)
    {
        fprintf(stderr, "Usage: %s dir_name\n", argv[0]);
        exit(1);
    }

    if ((dirp = opendir(argv[1])) == NULL)
    {
        perror(argv[1]);
        exit(2);
    }

    while ((direntp = readdir(dirp)) != NULL)
    {
        sprintf(fname, "%s/%s",argv[1],direntp->d_name);

        if (lstat(fname, &stat_buf) == -1) // testar com stat()
        {
            perror("lstat ERROR");
            exit(3);
        }
        if (S_ISREG(stat_buf.st_mode)){
            char aux[50];
            sprintf(aux, "regular - size=%ld B", stat_buf.st_size);
            str = aux;
        }
        else if (S_ISDIR(stat_buf.st_mode))
            str = "directory";
        else
            str = "other";
        printf("%-25s - inode=%ld - %s\n", direntp->d_name, direntp->d_ino, str);
    }

    closedir(dirp);
    exit(0);
}