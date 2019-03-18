#include <stdio.h> 
#include <stdlib.h> 
#include <string.h> 
#include <dirent.h> 
#include <sys/stat.h> 
#include <errno.h>

unsigned int mask = 0;
char outfile[25];

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

    printf("mask %x\n", mask);
    printf("outfile %s\n", outfile);
}

int main(int argc, char *argv[], char *envp[]) {

    init(argc, argv);

    return 0;
}