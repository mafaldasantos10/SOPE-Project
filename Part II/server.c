#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <pthread.h>

#include "utility.h"
#include "server.h"

#define MAX_SAVED_REQUESTS 20

int arrayIndex = 0;
bank_account_t bankAccounts[MAX_BANK_ACCOUNTS];
pthread_t thread[MAX_BANK_OFFICES];
tlv_request_t requestQueue[MAX_SAVED_REQUESTS];
int inputIndex = 0, outputIndex = 0;
int slots = MAX_SAVED_REQUESTS, requests = 0;

//Synchronitazion Mechanisms
pthread_mutex_t queueLock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t slotsLock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t requestsLock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t writeLock = PTHREAD_MUTEX_INITIALIZER;

pthread_cond_t slotsCond = PTHREAD_COND_INITIALIZER;
pthread_cond_t requestsCond = PTHREAD_COND_INITIALIZER;

void insertRequest(tlv_request_t *req)
{
    pthread_mutex_lock(&queueLock);
    requestQueue[inputIndex] = *req;
    inputIndex = (inputIndex + 1) % MAX_SAVED_REQUESTS;
    pthread_mutex_unlock(&queueLock);
}

void getRequest(tlv_request_t *req)
{
    pthread_mutex_lock(&queueLock);
    *req = requestQueue[outputIndex];
    outputIndex = (outputIndex + 1) % MAX_SAVED_REQUESTS;
    pthread_mutex_unlock(&queueLock);
}

void createAdmin(char *pass)
{
    bank_account_t account;
    char *salt = malloc(SALT_LEN);

    strcpy(salt, generateSalt());
    strcpy(account.hash, generateHash(salt, pass));
    strcpy(account.salt, salt);
    account.balance = 0;
    account.account_id = ADMIN_ACCOUNT_ID;
    bankAccounts[0] = account;

    free(salt);
}

void createBankOffices(int numThreads)
{
    int i;

    for (i = 1; i <= numThreads; i++)
    {
        if (pthread_create(&(thread[i]), NULL, consumeRequest, NULL) != 0)
        {
            perror("Creating Threads");
            exit(1);
        }
    }
}

int checkLoginAccount(req_header_t header)
{
    char *hash = malloc(HASH_LEN);

    for (int i = 0; i <= arrayIndex; i++)
    {
        hash = generateHash(bankAccounts[i].salt, header.password);

        if (header.account_id == bankAccounts[i].account_id && !strcmp(hash, bankAccounts[i].hash))
        {
            return i;
        }
    }

    return -1;
}

int searchID(uint32_t id)
{
    for (int i = 0; i <= arrayIndex; i++)
    {
        if (id == bankAccounts[i].account_id)
        {
            return id;
        }
    }

    return -1;
}

void createNewAccount(req_create_account_t newAccount, rep_header_t *sHeader)
{
    sHeader->account_id = newAccount.account_id;

    if (searchID(newAccount.account_id) > -1)
    {
        sHeader->ret_code = RC_SAME_ID;
        return;
    }

    arrayIndex++;
    bank_account_t account;
    char *salt = malloc(SALT_LEN);

    strcpy(salt, generateSalt());
    strcpy(account.hash, generateHash(salt, newAccount.password));
    strcpy(account.salt, salt);
    account.balance = newAccount.balance;
    account.account_id = newAccount.account_id;
    bankAccounts[arrayIndex] = account;
    sHeader->ret_code = RC_OK;

    free(salt);
}

void checkBalance(req_header_t header, rep_header_t *sHeader, rep_balance_t *sBalance)
{
    sHeader->account_id = header.account_id;
    int index = checkLoginAccount(header);
    printf("loginCheck %d, index \n", index);

    printf("account ballance - %d", bankAccounts[index].balance);
    fflush(stdout);
    sHeader->ret_code = RC_OK;
    sBalance->balance = bankAccounts[index].balance;
}

int transferChecks(req_value_t value, int index, rep_header_t *sHeader, rep_transfer_t *sTransfer)
{
    if (value.header.account_id == value.transfer.account_id)
    {
        sHeader->ret_code = RC_SAME_ID;
        sTransfer->balance = bankAccounts[index].balance;
        return 1;
    }

    if ((int)(bankAccounts[index].balance - value.transfer.amount) < (int)MIN_BALANCE)
    {
        sHeader->ret_code = RC_NO_FUNDS;
        sTransfer->balance = bankAccounts[index].balance;
        return 1;
    }

    return 0;
}

int finishTransfer(req_value_t value, int index, int i, rep_header_t *sHeader, rep_transfer_t *sTransfer)
{
    if (value.transfer.account_id == bankAccounts[i].account_id)
    {
        if (bankAccounts[i].balance + value.transfer.amount > MAX_BALANCE)
        {
            sHeader->ret_code = RC_TOO_HIGH;
            sTransfer->balance = bankAccounts[index].balance;
            return 1;
        }

        printf("previous amount 1 - %d \n", bankAccounts[index].balance);
        printf("previous amount 2 - %d \n", bankAccounts[i].balance);
        bankAccounts[i].balance += value.transfer.amount;
        bankAccounts[index].balance -= value.transfer.amount;
        printf("new amount 1 - %d \n", bankAccounts[index].balance);
        printf("new amount 2 - %d \n", bankAccounts[i].balance);
        fflush(stdout);
        sHeader->ret_code = RC_OK;
        sTransfer->balance = bankAccounts[index].balance;
        return 1;
    }

    return 0;
}

void bankTransfer(req_value_t value, rep_header_t *sHeader, rep_transfer_t *sTransfer)
{
    sHeader->account_id = value.header.account_id;
    int index = checkLoginAccount(value.header);

    if (transferChecks(value, index, sHeader, sTransfer)) //mudar nome da função
    {
        return;
    }

    fflush(stdout);

    for (int i = 0; i <= arrayIndex; i++)
    {
        if (finishTransfer(value, index, i, sHeader, sTransfer))
        {
            return;
        }
    }

    sHeader->ret_code = RC_ID_NOT_FOUND;
    sTransfer->balance = bankAccounts[index].balance;
}

int opUser(tlv_request_t tlv, rep_header_t *sHeader, rep_transfer_t *sTransfer)
{
    if (tlv.value.header.account_id == ADMIN_ACCOUNT_ID)
    {
        if (tlv.type == OP_BALANCE)
        {
            sHeader->ret_code = RC_OP_NALLOW;
            sHeader->account_id = tlv.value.header.account_id;
            return 1;
        }
        else if (tlv.type == OP_TRANSFER)
        {
            sHeader->ret_code = RC_OP_NALLOW;
            sHeader->account_id = tlv.value.header.account_id;
            sTransfer->balance = bankAccounts[ADMIN_ACCOUNT_ID].balance;
            return 1;
        }
    }

    if (tlv.value.header.account_id != ADMIN_ACCOUNT_ID)
    {
        if (tlv.type == OP_CREATE_ACCOUNT || tlv.type == OP_SHUTDOWN)
        {
            sHeader->ret_code = RC_OP_NALLOW;
            sHeader->account_id = tlv.value.header.account_id;
            return 1;
        }
    }

    return 0;
}

int requestHandler(tlv_request_t tlv, rep_header_t *sHeader, rep_balance_t *sBalance, rep_transfer_t *sTransfer, rep_value_t *sValue)
{
    if (checkLoginAccount(tlv.value.header) == -1)
    {
        fprintf(stderr, "Invalid password\n");
        sHeader->ret_code = RC_LOGIN_FAIL;
        return 0;
    }

    if (opUser(tlv, sHeader, sTransfer))
    {
        return 0;
    }

    switch (tlv.type)
    {
    case OP_CREATE_ACCOUNT:
        createNewAccount(tlv.value.create, sHeader);
        break;

    case OP_BALANCE:
        checkBalance(tlv.value.header, sHeader, sBalance);
        sValue->balance = *sBalance;
        break;

    case OP_TRANSFER:
        bankTransfer(tlv.value, sHeader, sTransfer);
        sValue->transfer = *sTransfer;
        break;

    case OP_SHUTDOWN:
        return -1;
        break;

    default:
        break;
    }

    return 0;
}

void *consumeRequest(void *arg)
{
    rep_header_t sHeader;
    rep_balance_t sBalance;
    rep_transfer_t sTransfer;
    rep_value_t sValue;
    tlv_reply_t *rTlv = (tlv_reply_t *)malloc(sizeof(struct tlv_reply));

    tlv_request_t tlv;
    int shutdown = 0;

    while (!shutdown)
    {
        pthread_mutex_lock(&requestsLock);
        while (requests <= 0)
            pthread_cond_wait(&requestsCond, &requestsLock);
        requests--;
        pthread_mutex_unlock(&requestsLock);

        getRequest(&tlv);

        if (requestHandler(tlv, &sHeader, &sBalance, &sTransfer, &sValue) == -1)
        {
            shutdown = 1;
        }
        else
        {
            sValue.header = sHeader;
            rTlv->value.header = sValue.header;
            rTlv->value.balance = sValue.balance;
            rTlv->value.transfer = sValue.transfer;
            rTlv->type = tlv.type;
            rTlv->length = sizeof(sValue) + sizeof(tlv.type);
        }

        char *pathFIFO = malloc(USER_FIFO_PATH_LEN);
        strcpy(pathFIFO, getFIFOName(tlv));

        pthread_mutex_lock(&writeLock);
        int userFIFO = open(pathFIFO, O_WRONLY | O_NONBLOCK);

        if (userFIFO < 0)
        {
            pthread_mutex_unlock(&writeLock);    
            if(!shutdown)
                perror("Could not open reply fifo");
            continue;
        }

        printf("\nid - %d\n", rTlv->value.header.account_id);
        printf("retorno - %d\n", rTlv->value.header.ret_code);
        fflush(stdout);
        write(userFIFO, rTlv, sizeof(*rTlv));
        close(userFIFO);
        usleep(10); //giving time for the user to close the fifo
        pthread_mutex_unlock(&writeLock);

        pthread_mutex_lock(&slotsLock);
        slots++;
        pthread_cond_signal(&slotsCond);
        pthread_mutex_unlock(&slotsLock);
    }

    printf("\nThread Finished!\n");
    pthread_exit(NULL);
}

void produceRequest(tlv_request_t tlv)
{
    pthread_mutex_lock(&slotsLock);
    while (slots <= 0)
        pthread_cond_wait(&slotsCond, &slotsLock);
    slots--;
    pthread_mutex_unlock(&slotsLock);

    insertRequest(&tlv);

    pthread_mutex_lock(&requestsLock);
    requests++;
    pthread_cond_signal(&requestsCond);
    pthread_mutex_unlock(&requestsLock);
}

char *getFIFOName(tlv_request_t tlv)
{
    int pid = tlv.value.header.pid;
    char *pathFIFO = malloc(USER_FIFO_PATH_LEN);
    sprintf(pathFIFO, "%s%d", USER_FIFO_PATH_PREFIX, pid);

    return pathFIFO;
}

void bankCycle(int numThreads)
{
    int serverFIFO = open(SERVER_FIFO_PATH, O_RDONLY);
    tlv_request_t tlv;
    int shutdown = 0;

    while (!shutdown)
    {
        if (read(serverFIFO, &tlv, sizeof(tlv_request_t)) > 0)
        {
            if (tlv.type == OP_SHUTDOWN && checkLoginAccount(tlv.value.header) != -1)
            {
                int it;
                for (it = 0; it < numThreads; it++)
                {
                    produceRequest(tlv);
                }
                shutdown = 1;
            }
            else
                produceRequest(tlv);
        }
    }
}

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        printf("Usage: %s numThreads password \n", argv[0]);
        exit(1);
    }

    int numThreads = atoi(argv[1]);

    validatePassword(argv[2]);

    createAdmin(argv[2]);

    createBankOffices(numThreads);

    mkfifo(SERVER_FIFO_PATH, 0666);

    bankCycle(numThreads);

    unlink(SERVER_FIFO_PATH);

    pthread_exit(NULL);
}