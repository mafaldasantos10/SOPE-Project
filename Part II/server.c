#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "types.h"
#include "user.h"

void generateSalt(char password[])
{
    char hex[] = "0123456789abcdef";
    char salt[SALT_LEN] = "";

    /* Seed number for rand() */
    srand(time(NULL));

    for (int i = 0; i < SALT_LEN; i++)
    {
        salt[i] = hex[rand() % 16];
    }

    //strcat(salt, password);
}