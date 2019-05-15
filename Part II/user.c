#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <sys/types.h>
#include <sys/times.h>
#include <sys/stat.h>

#include "types.h"
#include "user.h"
#include "utility.h"
#include "sope.h"

op_type_t op;

void init(int argc, char *argv[], tlv_request_t *tlv, req_create_account_t *createAccount, req_transfer_t *transfer, req_header_t *header, req_value_t *value)
{
    if (argc != 6)
    {
        printf("Usage: %s idConta password delay op{0,1,2,3} argList\n", argv[0]);
        exit(1);
    }

    op = atoi(argv[4]);

    fillOperationInfo(op, argv[5], createAccount, transfer, value);

    fillHeader(argv, header, value);

    tlv->type = op;
    tlv->value = *value;
    tlv->length = sizeof(value) + sizeof(op);
}

void fillHeader(char *argv[], req_header_t *header, req_value_t *value)
{
    header->pid = getpid();
    header->account_id = atoi(argv[1]);
    strcpy(header->password, argv[2]);
    validateDelay(argv[3]);
    header->op_delay_ms = atoi(argv[3]);

    value->header = *header;
}

void fillOperationInfo(int op, char *argList, req_create_account_t *createAccount, req_transfer_t *transfer, req_value_t *value)
{
    char *tok;

    switch (op)
    {
    case OP_CREATE_ACCOUNT: // accountID balance€ “password”

        tok = strtok(argList, " ");
        validateAccountID(tok);
        createAccount->account_id = atoi(tok);

        tok = strtok(NULL, " ");
        validateBalance(tok);
        createAccount->balance = atoi(tok);

        tok = strtok(NULL, " ");
        validatePassword(tok);
        strcpy(createAccount->password, tok);

        value->create = *createAccount;
        break;

    case OP_TRANSFER: // destinyID ammount€

        tok = strtok(argList, " ");
        validateAccountID(tok);
        transfer->account_id = atoi(tok);

        tok = strtok(NULL, " ");
        validateAmount(tok);
        transfer->amount = atoi(tok);

        value->transfer = *transfer;
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

int writeRequest(tlv_request_t *tlv, int fd)
{
    tlv_reply_t reply;
    int serverFIFO = open(SERVER_FIFO_PATH, O_WRONLY);

    logRequest(fd, getpid(), tlv);

    if (serverFIFO == -1)
    {
        reply.value.header.account_id = tlv->value.header.account_id;
        reply.value.header.ret_code = RC_SRV_DOWN;
        reply.type = op;
        reply.length = sizeof(reply.type) + sizeof(reply.value);
        logReply(fd, getpid(), &reply);

        printf("retorno - %d\n", reply.value.header.ret_code);
        fflush(stdout);

        return 0;
    }

    write(serverFIFO, tlv, sizeof(*tlv));
    close(serverFIFO);
    return 1;
}

void readReply(char *pathFIFO, int fd, int id)
{
    tlv_reply_t reply;
    int userFIFO;
    //Timeut measuremet
    double seconds;
    clock_t start, current;
    struct tms t;
    long ticks = sysconf(_SC_CLK_TCK);

    start = times(&t);

    userFIFO = open(pathFIFO, O_RDONLY | O_NONBLOCK);

    while (read(userFIFO, &reply, sizeof(reply)) <= 0)
    {
        current = times(&t);
        seconds = (double)(current - start) / ticks;
        if (seconds >= 30)
        {
            reply.value.header.account_id = id;
            reply.value.header.ret_code = RC_SRV_TIMEOUT;
            reply.type = op;
            reply.length = sizeof(reply.type) + sizeof(reply.value);

            break;
        }
    }

    printf("id - %d\n", reply.value.header.account_id);
    printf("retorno - %d\n", reply.value.header.ret_code);
    fflush(stdout);

    logReply(fd, getpid(), &reply);

    close(userFIFO);
}

int main(int argc, char *argv[])
{
    char *pathFIFO = malloc(USER_FIFO_PATH_LEN);
    tlv_request_t *tlv = (tlv_request_t *)malloc(sizeof(struct tlv_request));
    int fd = open(USER_LOGFILE, O_WRONLY | O_CREAT | O_APPEND, 0664);
    req_create_account_t createAccount;
    req_transfer_t transfer;
    req_header_t header;
    req_value_t value;

    strcpy(pathFIFO, getFIFOName());
    mkfifo(pathFIFO, 0666);

    init(argc, argv, tlv, &createAccount, &transfer, &header, &value);
    if (writeRequest(tlv, fd))
        readReply(pathFIFO, fd, header.account_id);

    close(fd);
    unlink(pathFIFO);

    return 0;
}