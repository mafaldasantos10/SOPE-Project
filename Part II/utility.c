#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>

#include "types.h"
#include "utility.h"

void validateAccountID(char *accountID)
{
    if (atoi(accountID) < 1 ||
        atoi(accountID) > MAX_BANK_ACCOUNTS ||
        strlen(accountID) != WIDTH_ACCOUNT)
    {
        printf("Incorrect Bank Account ID!\n");
        exit(1);
    }
}

void validateBalance(char *balance)
{
    if (atoi(balance) < MIN_BALANCE ||
        atoi(balance) > MAX_BALANCE ||
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
}

void validateAmount(char *amount)
{
    if (atoi(amount) < 1 ||
        atoi(amount) > MAX_BALANCE)
    {
        printf("Incorrect Amount!\n");
        exit(4);
    }
}