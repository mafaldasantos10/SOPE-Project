#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pthread.h>

#include "utility.h"
#include "server.h"

#define MAX_SAVED_REQUESTS 20

int accountsIndex = 0;
bank_account_t bankAccounts[MAX_BANK_ACCOUNTS];
bank_office_t offices[MAX_BANK_OFFICES];
int activeThreads = 0;

tlv_request_t requestQueue[MAX_SAVED_REQUESTS];
int inputIndex = 0, outputIndex = 0;
int slots = MAX_SAVED_REQUESTS, requests = 0;

int shutdown = 0;

//Synchronitazion Mechanisms
pthread_mutex_t accountLocks[MAX_BANK_ACCOUNTS];
pthread_mutex_t queueLock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t slotsLock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t requestsLock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t writeLock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t activeLock = PTHREAD_MUTEX_INITIALIZER;

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

    offices[0].bankID = 0;
    offices[0].tid = pthread_self();

    free(salt);
}

void createBankOffices(int numThreads)
{
    int i;

    for (i = 1; i <= numThreads; i++)
    {
        offices[i].bankID = i;
        if (pthread_create(&(offices[i].tid), NULL, consumeRequest, NULL) != 0)
        {
            perror("Creating Threads");
            exit(1);
        }
    }
}

int checkLoginAccount(req_header_t header)
{
    char *hash = malloc(HASH_LEN);

    for (int i = 0; i <= accountsIndex; i++)
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
    for (int i = 0; i <= accountsIndex; i++)
    {
        if (id == bankAccounts[i].account_id)
        {
            return id;
        }
    }

    return -1;
}

void createNewAccount(req_value_t value, rep_header_t *sHeader)
{
    bank_account_t account;
    req_create_account_t newAccount = value.create;

    sHeader->account_id = newAccount.account_id;

    if (searchID(newAccount.account_id) > -1)
    {
        sHeader->ret_code = RC_ID_IN_USE;
        return;
    }

    char *salt = malloc(SALT_LEN);
    accountsIndex++;
    pthread_mutex_init(&accountLocks[accountsIndex], NULL);

    strcpy(salt, generateSalt());
    strcpy(account.hash, generateHash(salt, newAccount.password));
    strcpy(account.salt, salt);
    account.balance = newAccount.balance;
    account.account_id = newAccount.account_id;

    pthread_mutex_lock(&accountLocks[accountsIndex]);
    usleep(value.header.op_delay_ms * 1000);
    bankAccounts[accountsIndex] = account;
    pthread_mutex_unlock(&accountLocks[accountsIndex]);

    sHeader->ret_code = RC_OK;

    free(salt);
}

void checkBalance(req_header_t header, rep_header_t *sHeader, rep_balance_t *sBalance)
{
    sHeader->account_id = header.account_id;
    int index = checkLoginAccount(header);
    printf("loginCheck %d, index \n", index);

    sHeader->ret_code = RC_OK;
    pthread_mutex_lock(&accountLocks[index]);
    usleep(header.op_delay_ms * 1000);
    printf("account ballance - %d", bankAccounts[index].balance);
    fflush(stdout);
    sBalance->balance = bankAccounts[index].balance;
    pthread_mutex_unlock(&accountLocks[index]);
}

int isValidTransfer(req_value_t value, int index, rep_header_t *sHeader, rep_transfer_t *sTransfer)
{
    if (value.header.account_id == value.transfer.account_id)
    {
        sHeader->ret_code = RC_SAME_ID;
        pthread_mutex_lock(&accountLocks[index]);
        sTransfer->balance = bankAccounts[index].balance;
        pthread_mutex_unlock(&accountLocks[index]);
        return 1;
    }

    pthread_mutex_lock(&accountLocks[index]);
    if ((int)(bankAccounts[index].balance - value.transfer.amount) < (int)MIN_BALANCE)
    {
        sHeader->ret_code = RC_NO_FUNDS;
        sTransfer->balance = bankAccounts[index].balance;
        pthread_mutex_unlock(&accountLocks[index]);
        return 1;
    }
    pthread_mutex_unlock(&accountLocks[index]);

    return 0;
}

int finishTransfer(req_value_t value, int source, int dest, rep_header_t *sHeader, rep_transfer_t *sTransfer)
{
    if (value.transfer.account_id == bankAccounts[dest].account_id)
    {
        pthread_mutex_lock(&accountLocks[dest]);
        if (bankAccounts[dest].balance + value.transfer.amount > MAX_BALANCE)
        {
            pthread_mutex_unlock(&accountLocks[dest]);
            sHeader->ret_code = RC_TOO_HIGH;
            pthread_mutex_lock(&accountLocks[source]);
            sTransfer->balance = bankAccounts[source].balance;
            pthread_mutex_unlock(&accountLocks[source]);
            return 1;
        }
        pthread_mutex_unlock(&accountLocks[dest]);

        printf("previous amount 1 - %d \n", bankAccounts[source].balance);
        printf("previous amount 2 - %d \n", bankAccounts[dest].balance);
        pthread_mutex_lock(&accountLocks[dest]);
        usleep(value.header.op_delay_ms * 1000);
        bankAccounts[dest].balance += value.transfer.amount;
        pthread_mutex_unlock(&accountLocks[dest]);
        pthread_mutex_lock(&accountLocks[source]);
        usleep(value.header.op_delay_ms * 1000);
        bankAccounts[source].balance -= value.transfer.amount;
        pthread_mutex_unlock(&accountLocks[source]);
        printf("new amount 1 - %d \n", bankAccounts[source].balance);
        printf("new amount 2 - %d \n", bankAccounts[dest].balance);
        fflush(stdout);

        sHeader->ret_code = RC_OK;
        pthread_mutex_lock(&accountLocks[source]);
        sTransfer->balance = bankAccounts[source].balance;
        pthread_mutex_unlock(&accountLocks[source]);
        return 1;
    }

    return 0;
}

void bankTransfer(req_value_t value, rep_header_t *sHeader, rep_transfer_t *sTransfer)
{
    sHeader->account_id = value.header.account_id;
    int index = checkLoginAccount(value.header);

    if (isValidTransfer(value, index, sHeader, sTransfer))
    {
        return;
    }

    fflush(stdout);

    for (int i = 0; i <= accountsIndex; i++)
    {
        if (finishTransfer(value, index, i, sHeader, sTransfer))
        {
            return;
        }
    }

    sHeader->ret_code = RC_ID_NOT_FOUND;
    pthread_mutex_lock(&accountLocks[index]);
    sTransfer->balance = bankAccounts[index].balance;
    pthread_mutex_unlock(&accountLocks[index]);
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
        createNewAccount(tlv.value, sHeader);
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
        usleep(tlv.value.header.op_delay_ms*1000);
        chmod(SERVER_FIFO_PATH, 0444);
        sHeader->account_id = tlv.value.header.account_id;
        sHeader->ret_code = RC_OK;
        sValue->shutdown.active_offices = activeThreads;
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

    while (1)
    {
        pthread_mutex_lock(&requestsLock);
        if (shutdown && requests <= 0)
        {
            pthread_cond_broadcast(&requestsCond);
            pthread_mutex_unlock(&requestsLock);
            break;
        }

        while (requests <= 0)
        {
            pthread_cond_wait(&requestsCond, &requestsLock);
            if (shutdown)
            {
                printf("\nThread Finished!\n");
                pthread_mutex_unlock(&requestsLock);
                pthread_exit(NULL);
            }
        }
        requests--;
        pthread_mutex_unlock(&requestsLock);

        getRequest(&tlv);

        pthread_mutex_lock(&activeLock);
        activeThreads++;
        pthread_mutex_unlock(&activeLock);

        if (requestHandler(tlv, &sHeader, &sBalance, &sTransfer, &sValue) == -1)
        {
            shutdown = 1;
        }
        sValue.header = sHeader;
        rTlv->value.header = sValue.header;
        rTlv->value.balance = sValue.balance;
        rTlv->value.transfer = sValue.transfer;
        rTlv->type = tlv.type;
        rTlv->length = sizeof(sValue) + sizeof(tlv.type);

        char *pathFIFO = malloc(USER_FIFO_PATH_LEN);
        strcpy(pathFIFO, getUserFifo(tlv));

        pthread_mutex_lock(&writeLock);
        int userFIFO = open(pathFIFO, O_WRONLY | O_NONBLOCK);

        if (userFIFO == -1)
        {
            pthread_mutex_unlock(&writeLock);
            // if (!shutdown)
            // {
            perror("Could not open reply fifo");
            // }
            continue;
        }

        printf("\nid - %d\n", rTlv->value.header.account_id);
        printf("retorno - %d\n", rTlv->value.header.ret_code);
        fflush(stdout);
        write(userFIFO, rTlv, sizeof(*rTlv));
        close(userFIFO);

        // if(shutdown)
        //     usleep(10); //giving time for the user to close the fifo

        pthread_mutex_unlock(&writeLock);

        pthread_mutex_lock(&activeLock);
        activeThreads--;
        pthread_mutex_unlock(&activeLock);

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

char *getUserFifo(tlv_request_t tlv)
{
    int pid = tlv.value.header.pid;
    char *pathFIFO = malloc(USER_FIFO_PATH_LEN);
    sprintf(pathFIFO, "%s%d", USER_FIFO_PATH_PREFIX, pid);

    return pathFIFO;
}

void bankCycle()
{
    int serverFIFO = open(SERVER_FIFO_PATH, O_RDONLY);
    tlv_request_t tlv;

    while (1)
    {
        if (read(serverFIFO, &tlv, sizeof(tlv_request_t)) > 0)
        {
            produceRequest(tlv);
        }
        else if (shutdown)
        {
            break;
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

    bankCycle();

    unlink(SERVER_FIFO_PATH);

    pthread_exit(NULL);
}