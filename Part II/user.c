#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
//#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "types.h"
#include "user.h"

op_type_t op;
req_create_account_t createAccount;
req_transfer_t transfer;

void init(int argc, char *argv[])
{
    if (argc != 6)
    {
        printf("Usage: %s idConta password delay op{0,1,2,3} argList\n", argv[0]);
        exit(1);
    }

    op = atoi(argv[4]);

    fillOperationInfo(op, argv[5]);
}

void fillOperationInfo(int op, char *argList)
{
    char *tok;

    switch (op)
    {
    case OP_CREATE_ACCOUNT: // accountID balance€ “password”

        tok = strtok(argList, " ");
        createAccount.account_id = atoi(tok);

        tok = strtok(NULL, " ");
        createAccount.balance = atoi(tok);

        tok = strtok(NULL, " ");
        strcpy(createAccount.password, tok);
        break;

    case OP_TRANSFER: // destinyID ammount€

        tok = strtok(argList, " ");
        transfer.account_id = atoi(tok);

        tok = strtok(NULL, " ");
        transfer.amount = atoi(tok);
        printf("id %d ammount %d\n", transfer.account_id, transfer.amount);
        break;

    default:
        break;
    }
}

int main(int argc, char *argv[])
{
    init(argc, argv);
}