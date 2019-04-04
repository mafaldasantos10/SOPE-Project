#pragma once

#include <string.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>

//Needed because stdout_fileno will be replaced by outfile fd
static int stdout_fd = STDOUT_FILENO;

void set_fd(int fd)
{
    stdout_fd = fd;
}

void close_fd()
{
    close(stdout_fd);
}

int blockSigint()
{
    sigset_t mask;

    if (sigprocmask(0, NULL, &mask))
    {
        return -1;
    }
    else
    {
        sigaddset(&mask, SIGINT);
        if (sigprocmask(SIG_SETMASK, &mask, NULL))
        {
            return -1;
        }
    }

    return 0;
}

int unblockSigint()
{
    sigset_t mask;

    if (sigprocmask(0, NULL, &mask))
    {
        return -1;
    }
    else
    {
        sigdelset(&mask, SIGINT);
        if (sigprocmask(SIG_SETMASK, &mask, NULL))
        {
            return -1;
        }
    }

    return 0;
}

void sig_usr(int sig)
{
    static int numDirs = 0;
    static int numFiles = 0;
    char str[512];

    if (sig == SIGUSR1)
    {
        numDirs++;
        sprintf(str, "New directory: %d/%d directories/files at this time.\n", numDirs, numFiles);
        write(stdout_fd, str, strlen(str));
    }
    else if (sig == SIGUSR2)
    {
        numFiles++;
    }
    else
    {
        sprintf(str, "Finished: %d/%d directories/files analised.\n", numDirs, numFiles);
        write(stdout_fd, str, strlen(str));
    }
    
}