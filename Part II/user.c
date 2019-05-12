#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>

#include "types.h"
#include "user.h"
#include "utility.h"

op_type_t op;
req_create_account_t createAccount;
req_transfer_t transfer;
req_header_t header;
req_value_t value;

void init(int argc, char *argv[], tlv_request_t *tlv)
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
    tlv->length = sizeof(value) + sizeof(op);
}

void fillHeader(char *argv[])
{
    header.pid = getpid();
    header.account_id = atoi(argv[1]);
    strcpy(header.password, argv[2]);
    validateDelay(argv[3]);
    header.op_delay_ms = atoi(argv[3]);

    value.header = header;
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

char *getFIFOName()
{
    int pid = getpid();
    char *pathFIFO = malloc(USER_FIFO_PATH_LEN);
    sprintf(pathFIFO, "%s%d", USER_FIFO_PATH_PREFIX, pid);
    printf("\ncodigo do fifo - %s\n", pathFIFO);
    fflush(stdout);

    return pathFIFO;
}

int main(int argc, char *argv[])
{
    char *pathFIFO = malloc(USER_FIFO_PATH_LEN);
    strcpy(pathFIFO, getFIFOName());
    mkfifo(pathFIFO, 0666);
    tlv_request_t *tlv = (tlv_request_t *)malloc(sizeof(struct tlv_request));
    tlv_reply_t rTlv;

    init(argc, argv, tlv);

    int serverFIFO = open(SERVER_FIFO_PATH, O_WRONLY);

    if (serverFIFO == -1)
    {
        printf("sync error ");
        return 1;
    }

    write(serverFIFO, tlv, sizeof(*tlv));
    close(serverFIFO);

    int userFIFO = open(pathFIFO, O_RDONLY);

    if (read(userFIFO, &rTlv, sizeof(rTlv)) > 0)
    {
        printf("id - %d\n", rTlv.value.header.account_id);
        printf("retorno - %d\n", rTlv.value.header.ret_code);
        fflush(stdout);
    }

    close(userFIFO);
    unlink(pathFIFO);

    return 0;
}