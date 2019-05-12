#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "types.h"
#include "user.h"
#include "utility.h"

op_type_t op;
req_create_account_t createAccount;
req_transfer_t transfer;
req_header_t header;
req_value_t value;

void init(int argc, char *argv[], tlv_request_t* tlv)
{
    if (argc != 6)
    {
        printf("Usage: %s idConta password delay op{0,1,2,3} argList\n", argv[0]);
        exit(1);
    }

    op = atoi(argv[4]);

    fillOperationInfo(op, argv[5]);

    fillHeader(argv);

    tlv->type = op;
    tlv->value = value;
    tlv->length =  sizeof(value) + sizeof(op);

}

void fillHeader(char *argv[])
{
    header.pid = getpid();
    header.account_id = atoi(argv[1]);
    strcpy(header.password, argv[2]);
    validateDelay(argv[3]);
    header.op_delay_ms = atoi(argv[3]);

    value.header= header;
}

void fillOperationInfo(int op, char *argList)
{
    char *tok;

    switch (op)
    {
    case OP_CREATE_ACCOUNT: // accountID balance€ “password”

        tok = strtok(argList, " ");
        validateAccountID(tok);
        createAccount.account_id = atoi(tok);

        tok = strtok(NULL, " ");
        printf("%s", tok);
        validateBalance(tok);
        createAccount.balance = atoi(tok);

        tok = strtok(NULL, " ");
        validatePassword(tok);
        strcpy(createAccount.password, tok);

        value.create = createAccount;
        break;

    case OP_TRANSFER: // destinyID ammount€

        tok = strtok(argList, " ");
        validateAccountID(tok);
        transfer.account_id = atoi(tok);

        tok = strtok(NULL, " ");
        validateAmount(tok);
        transfer.amount = atoi(tok);

        value.transfer = transfer;
        break;

    default:
        break;
    }
}

int main(int argc, char *argv[])
{
    tlv_request_t* tlv = (tlv_request_t *) malloc(sizeof(struct tlv_request));
    init(argc, argv, tlv);

    int fifo = open(SERVER_FIFO_PATH, O_WRONLY);
    if (fifo == -1)
    {
        printf("burroooon %d", errno);
        perror("");
        return 1;
    }

    write(fifo, tlv, sizeof(*tlv));//talvez malloc
    printf("%s",tlv->value.header.password);
    printf("escrevi \n");
    close(fifo);

    return 0;
}