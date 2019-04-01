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
#include <dirent.h>
#include "utility.h"

opt_t options;

log_t logF;

void writeLog(char *act);

/** @brief Initializes the program. Checks for invalid inputs and parses execution options (setting opt_t fields to true)
 * 
 *  @param argc Number of arguments (directly from main)
 *  @param argv Array containing the arguments (directly from main)
*/
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
            options.r = 1;

        if (!strcmp(argv[i - 1], "-h"))
        {
            token = strtok(argv[i], ",");

            while (token != NULL)
            {

                if (!strcmp(token, "md5"))
                    options.md5 = 1;

                if (!strcmp(token, "sha1"))
                    options.sha1 = 1;

                if (!strcmp(token, "sha256"))
                    options.sha256 = 1;

                token = strtok(NULL, ",");
            }
        }

        if (!strcmp(argv[i - 1], "-o"))
        {
            int out = open(argv[i], O_WRONLY | O_CREAT | O_EXCL | O_APPEND | O_SYNC, 0664);
            if (out < 0)
            {
                perror(argv[i]);
                exit(1);
            }
            printf("Data saved on file %s\n", argv[i]);
            dup2(out, STDOUT_FILENO);
            options.o = 1;
        }

        if (!strcmp(argv[i], "-v"))
        {
            char *command = malloc(sizeof(char) * 1024);
            strcpy(command, "COMMAND");
            logF.logFile = getenv("LOGFILENAME");
            logF.fileDescriptor = open(logF.logFile, O_WRONLY | O_CREAT | O_APPEND, 0664);

            for (int i = 0; i < argc; i++)
            {
                strcat(strcat(command, " "), argv[i]);
            }

            strcat(command, "\n");
            writeLog(command);
            options.v = 1;
        }
    }
}

/** @brief  writes in the log file in the format -inst -pid -act
 * 
 * @param  act to be printed
 */
void writeLog(char *act)
{
    char *inf = malloc(sizeof(char) * 1024);

    long ticksPS = sysconf(_SC_CLK_TCK);
    logF.end = times(&logF.time);

    double currentTime = ((double)logF.end - logF.start) / ticksPS * 1000;

    sprintf(inf, "%.2f - %.8d - %s", currentTime, getpid(), act);

    //printf("string %s \n", inf);
    write(logF.fileDescriptor, inf, strlen(inf));

    free(inf);
}

/** @brief Runs a given shell command in the format "command filename". Its output is sent to out_buffer
 * 
 *  @param command Command name
 *  @param filename File name
 *  @param out_buffer String to where the output will be sent
 * 
 *  @return Number of chars written to out_buffer
 */
int runCommand(char command[], char filename[], char out_buffer[])
{
    int pipe_fd[2];
    pid_t pid;

    if (pipe(pipe_fd) < 0)
    {
        perror("runCommand: pipe");
        exit(1);
    }

    pid = fork();

    if (pid != 0) // PARENT
    {
        close(pipe_fd[WRITE]);
        wait(NULL);
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

    return read(pipe_fd[READ], out_buffer, BUFFER_SIZE);
}

/** @brief Prints the first two fields of 'file' command output
 * 
 *  @param buffer String (char array) holding 'file's output.
 */
void printFileCmd(char buffer[])
{
    char *token;

    token = strtok(buffer, ":");
    printf("%s", token);
    token = strtok(NULL, ",\n");
    //token is incremented to avoid printing a space character after the comma
    printf(",%s", ++token);
}

/** @brief Prints the cryptographic summary resulting of a command (either md5sum, sha1sum or sha256sum)
 * 
 *  @param buffer String (char array) holding the command output.
 */
void printSum(char buffer[])
{
    char *token;

    token = strtok(buffer, " ");
    printf(",%s", token);
}

/** @brief Prints the stats of a file, according to the inputed options.
 * 
 *  @param fileStat Struct stat variable with the file properties
 *  @param filename Name of the file whose stats will be printed
 */
void printStats(struct stat fileStat, char filename[])
{
    char str[BUFFER_SIZE];

    //printf("NAME %s \n", str);
    runCommand("file", filename, str);
    printFileCmd(str);
    printf(",%lu", fileStat.st_size);
    printf(",%s", formatPermissions(str, fileStat.st_mode));
    printf(",%s", formatDate(str, fileStat.st_atime));
    printf(",%s", formatDate(str, fileStat.st_mtime));

    if (options.md5)
    {
        runCommand("md5sum", filename, str);
        printSum(str);
    }

    if (options.sha1)
    {
        runCommand("sha1sum", filename, str);
        printSum(str);
    }

    if (options.sha256)
    {
        runCommand("sha256sum", filename, str);
        printSum(str);
    }

    printf("\n");
}

/** @brief Rearranges the char to be printed in the log file
 * 
 * @param information to be printed
 */
void newFile(char *inf)
{
    char *log = malloc(sizeof(char) * 1024);
    sprintf(log, "%s%s %s", "ANALIZED ", inf, "\n");
    writeLog(log);
    free(log);
}

/** @brief Analyses the directory and its subdirectories to get every file stats
 * 
 *  @param directory Directory name
 */
void readDirectory(char *directory)
{
    DIR *dir;
    struct dirent *dirent;
    struct stat fileStat;
    char filePath[258];

    if (lstat(directory, &fileStat))
    {
        perror(directory);
        exit(1);
    }

    if (S_ISDIR(fileStat.st_mode))
    {
        if ((dir = opendir(directory)) == NULL)
        {
            perror(directory);
            exit(2);
        }

        while ((dirent = readdir(dir)) != NULL)
        {
            sprintf(filePath, "%s/%s", directory, dirent->d_name);

            if (lstat(filePath, &fileStat) == -1)
            {
                perror(directory);
                exit(3);
            }

            if (S_ISDIR(fileStat.st_mode))
            {
                if (notPoint(dirent->d_name) && options.r)
                {
                    newFile(dirent->d_name);
                    pid_t pid = fork();

                    if (pid > 0)
                    {
                        wait(NULL);
                    }
                    else if (pid == 0)
                    {
                        readDirectory(filePath);
                        break;
                    }
                }
            }
            else
            {
                newFile(dirent->d_name);
                printStats(fileStat, filePath);
            }
        }
    }
    else
    {
        newFile(directory);
        printStats(fileStat, directory);
    }
}

int main(int argc, char *argv[])
{
    logF.start = times(&logF.time);

    init(argc, argv);

    readDirectory(argv[argc - 1]);
    //writeLog();

    return 0;
}