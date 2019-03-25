#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <errno.h>
#include <time.h>
#include "utility.h"

#define READ 0
#define WRITE 1
#define BUFFER_SIZE 512

char outfile[25];
Mode mode;

/** @brief ... */
void init(int argc, char *argv[])
{
    char *token;

    if (argc < 2)
    {
        printf("Usage: %s [-r] [-h [md5[,sha1[,sha256]]]] [-o <outfile>] [-v] <file|dir>\n", argv[0]);
        exit(1);
    }

    for (int i = 1; i < argc - 1; i++)
    {
        if (!strcmp(argv[i], "-r"))
            mode.r = 1;

        if (!strcmp(argv[i - 1], "-h"))
        {
            token = strtok(argv[i], ",");

            while (token != NULL)
            {

                if (!strcmp(token, "md5"))
                    mode.md5 = 1;

                if (!strcmp(token, "sha1"))
                    mode.sha1 = 1;

                if (!strcmp(token, "sha256"))
                    mode.sha256 = 1;

                token = strtok(NULL, ",");
            }
        }

        if (!strcmp(argv[i - 1], "-o"))
        {
            strcpy(outfile, argv[i]);
            mode.o = 1;
        }

        if (!strcmp(argv[i], "-v"))
            mode.v = 1;
    }
}

/** @brief returns a string with the date complying with the ISO 8601 format, <date>T<time> */
char *formatDate(char *s, time_t val)
{
    strftime(s, 50, "%Y-%m-%dT%X", localtime(&val));
    return s;
}

char *formatPerm(char *s, int st_mode)
{
    char str[4];
    int i = -1;
    if (st_mode & S_IRUSR){    //User has reading permission
        str[++i] = 'r';
    }
    if (st_mode & S_IWUSR){    //User has writing permission
        str[++i] = 'w';
    }
    if (st_mode & S_IXUSR){   //User has executing permission
        str[++i] = 'x';
    }

    str[++i] = '\0';

    return strcpy(s, str);
}

/** @brief runs a given shell command in the format "command filename" */
void runCommand(char command[], char filename[])
{
    char buffer[BUFFER_SIZE];
    char *token;
    int pipe_fd[2];             //pipe used for redirecting the output of the command
    int out_fd = STDOUT_FILENO; //By default, the output will go to stdout
    pid_t pid;

    /* setting the communication pipe */
    if (pipe(pipe_fd) < 0)
    {
        perror("runCommand: pipe");
        exit(1);
    }

    /* runs the command */
    pid = fork();

    if (pid != 0) // PARENT
    {
        close(pipe_fd[WRITE]);
        wait(NULL); /* wait for the child to terminate */
    }
    else if (pid == 0) // CHILD
    {
        close(pipe_fd[READ]);
        dup2(pipe_fd[WRITE], STDOUT_FILENO);
        if (!strcmp(command, "file"))
            //option '-p' preserves access date so our forensic analysis doesn't change anything on analised files and dirs
            execlp(command, command, "-p", filename, NULL);
        else
            execlp(command, command, filename, NULL);
        perror("runCommand");
        exit(2);
    }

    /* prints the command output */
    read(pipe_fd[READ], buffer, BUFFER_SIZE);
    if (!strcmp(command, "file"))
    {
        /* printing first two fields of file command*/
        token = strtok(buffer, ":");
        write(out_fd, token, strlen(token));
        token = strtok(NULL, ",\n");
        token[0] = ','; //swapping the first space in the 2nd field of file's output for a colon
        write(out_fd, token, strlen(token));
    }
    else
    {
        token = strtok(buffer, " "); /* only selects the argument before the ' ' (space), i.e. the actual cripto */
        write(out_fd, token, strlen(token));
    }
}

/** @brief prints the stats of a file */
void printStats(struct stat fileStat, char filename[])
{

    char date[50];

    runCommand("file", filename);                       /* file name and type */
    printf(",%lu", fileStat.st_size);                   /* file size */
    printf(",%s", formatPerm(date, fileStat.st_mode));  /* file protection */
    printf(",%s", formatDate(date, fileStat.st_atime)); /* time of last access */
    printf(",%s", formatDate(date, fileStat.st_mtime)); /* time of last modification */

    /* md5 */
    //If bit is set, numerical value will be greater than 0 -> true
    if (mode.md5)
        runCommand("md5sum", filename);

    /* sha1 */
    if (mode.sha1)
        runCommand("sha1sum", filename);

    /* sha256 */
    if (mode.sha256)
        runCommand("sha256sum", filename);

    printf("\n");
}

int main(int argc, char *argv[], char *envp[])
{

    struct stat fileStat;

    init(argc, argv);

    if (stat(argv[argc - 1], &fileStat))
    {
        perror("Stat");
        exit(1);
    }

    printStats(fileStat, argv[argc - 1]);

    return 0;
}