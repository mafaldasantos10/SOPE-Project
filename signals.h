#pragma once

#include <signal.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

//Needed because stdout_fileno will be replaced by outfile fd
static int stdout_fd = STDOUT_FILENO;

void set_fd(int fd){
    stdout_fd = fd;
}

// void sig_int(int sig){
//     if(sig != SIGINT){
//         perror("Bad Signal");
//     }
// }

void sig_usr(int sig){
    static int numDirs = 0;
    static int numFiles = 0;
    char str[512];

    if(sig == SIGUSR1){
        numDirs++;
        sprintf(str, "New directory: %d/%d directories/files at this time.\n", numDirs, numFiles);
        write(stdout_fd, str, strlen(str));
    }
    else {
        numFiles++;
    }
}