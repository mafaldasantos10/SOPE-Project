#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <sys/types.h>

#include "types.h"
#include "utility.h"

/*
 * ================================================================================================
 * USER Related Utility
 * ================================================================================================
 */

void validateAccountID(char *accountID)
{
    if (atoi(accountID) < 0 ||
        (uint32_t)atoi(accountID) > MAX_BANK_ACCOUNTS ||
        strlen(accountID) > WIDTH_ACCOUNT)
    {
        printf("Incorrect Bank Account ID!\n");
        exit(1);
    }
}

void validateBalance(char *balance)
{
    if ((uint32_t)atoi(balance) < MIN_BALANCE ||
        (uint32_t)atoi(balance) > MAX_BALANCE ||
        strlen(balance) > WIDTH_BALANCE)
    {
        printf("Incorrect Balance!\n");
        exit(2);
    }
}

void validatePassword(char *pass)
{
    if (strlen(pass) < MIN_PASSWORD_LEN ||
        strlen(pass) > MAX_PASSWORD_LEN)
    {
        printf("The password must have between 8 and 20 characters!\n");
        exit(3);
    }

    for (uint32_t i = 0; i < strlen(pass); i++)
    {
        if (pass[i] == ' ')
        {
            printf("No [spaces] allowed on the password!\n");
            exit(3);
        }
    }
}

void validateAmount(char *amount)
{
    if (atoi(amount) < 1 ||
        (uint32_t)atoi(amount) > MAX_BALANCE)
    {
        printf("Incorrect Amount!\n");
        exit(4);
    }
}

void validateDelay(char *delay)
{
    if (atoi(delay) < 1 ||
        (uint32_t)atoi(delay) > MAX_OP_DELAY_MS)
    {
        printf("Incorrect Delay!\n");
        exit(5);
    }
}

/*
 * ================================================================================================
 * SERVER Related Utility
 * ================================================================================================
 */

char *generateSalt()
{
    char hex[16] = "0123456789abcdef";
    char *salt = malloc(SALT_LEN + 1);

    /* New seed for rand() */
    srand(time(NULL));

    for (int i = 0; i < SALT_LEN + 1; i++)
    {
        salt[i] = hex[rand() % 16];
    }

    salt[SALT_LEN] = '\0';
    return salt;
}

char *generateHash(char *salt, char *password)
{
    FILE *fpout;
    char *hash = malloc(HASH_LEN);
    char *cmd = malloc(sizeof("echo -n | sha256sum") + MAX_PASSWORD_LEN + SALT_LEN + 1);

    sprintf(cmd, "echo -n %s%s | sha256sum", password, salt);

    fpout = popen(cmd, "r");

    if(fpout == NULL){
        perror("Generating Hash");
        exit(2);
    }
    fgets(hash, HASH_LEN, fpout);
    pclose(fpout);

    return hash;
}