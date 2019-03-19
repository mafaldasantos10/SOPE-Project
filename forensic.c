#include <stdio.h> 
#include <stdlib.h> 
#include <string.h> 
#include <dirent.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include <time.h>

unsigned int mask = 0;
char outfile[25];

/** @brief ... */
void init(int argc, char *argv[]) {

    char *token;

    if (argc < 2) {
        printf("Too few arguments!\n");
        exit(1);
    }

    for (int i = 1; i < argc - 1; i++) {
        if (!strcmp(argv[i], "-r"))
            mask |= 0x20;

        if (!strcmp(argv[i-1], "-h")) {
            token = strtok(argv[i], ",");

            while (token != NULL) {

                if (!strcmp(token, "md5"))
                    mask |= 0x10;

                if (!strcmp(token, "sha1"))
                    mask |= 0x08;

                if (!strcmp(token, "sha256"))
                    mask |= 0x04;

                token = strtok(NULL, ",");
            }
        }

        if (!strcmp(argv[i-1], "-o")) {
            strcpy(outfile, argv[i]);
            mask |= 0x02;
        }

        if (!strcmp(argv[i], "-v"))
            mask |= 0x01;
    }
}

/** @brief returns a string with the date complying with the ISO 8601 format, <date>T<time> */
char* formatDate(char* s, time_t val)
{
    strftime(s, 50, "%Y-%m-%dT%X", localtime(&val));
    return s;
}

/** @brief runs a given shell command in the format "command filename" */
void runCommand(char command[], char filename[]) {
    
    FILE *fp;
    char buffer[250];

    /* shell command "file 'name file'" */
    sprintf(buffer, "%s %s", command, filename);

    /* runs the command */
    fp = popen(buffer, "r");
    if (fp == NULL) {
        printf("Invalid command\n" );
        exit(1);
    }

    /* prints the command output */
    fgets(buffer, sizeof(buffer)-1, fp);
    if (!strcmp(command, "file"))
        printf("%s", strtok(buffer, ",")); /* only prints the argument before the ',' */
    else
        printf(",%s", strtok(buffer, " ")); /* only prints the argument before the ' ' (space) */
    
    /* close */
    pclose(fp);
}

/** @brief prints some stats of a file */
void printStats(struct stat fileStat, char filename[]) {

    char date[50];
    int temp;
    
    runCommand("file", filename);

    printf(",%lu", fileStat.st_size); /* file size */
    printf(",%x", fileStat.st_mode); /* file protection */
    printf(",%s", formatDate(date, fileStat.st_atime)); /* time of last access */
    printf(",%s", formatDate(date, fileStat.st_mtime)); /* time of last modification */

    /* md5 */
    if (((temp = mask & 0x10)>> 4) == 1)
        runCommand("md5sum", filename);
        
    /* sha1 */
    if (((temp = mask & 0x08)>> 3) == 1)
        runCommand("sha1sum", filename);

    /* sha256 */
    if (((temp = mask & 0x04)>> 2) == 1)
        runCommand("sha256sum", filename);
}

int main(int argc, char *argv[], char *envp[]) {
    
    struct stat fileStat;

    init(argc, argv);

    if (stat(argv[argc-1], &fileStat)) {
        perror("Stat");
        exit(1);
    }

    printStats(fileStat, argv[argc - 1]);

    return 0;
}