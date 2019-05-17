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
#include "sope.h"
#include "server.h"

#define MAX_SAVED_REQUESTS 20

int logfd;

int accountsIndex = 0;
bank_account_t bankAccounts[MAX_BANK_ACCOUNTS];
bank_office_t offices[MAX_BANK_OFFICES];
int numThreads = 0;
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
pthread_mutex_t activeLock = PTHREAD_MUTEX_INITIALIZER;

pthread_cond_t slotsCond = PTHREAD_COND_INITIALIZER;
pthread_cond_t requestsCond = PTHREAD_COND_INITIALIZER;

void insertRequest(const tlv_request_t *req)
{
    if (logSyncMech(logfd, 0, SYNC_OP_MUTEX_LOCK, SYNC_ROLE_PRODUCER, req->value.header.pid) < 0)
    {
        fprintf(stderr, "Error writing to log. Moving on...\n");
    }
    pthread_mutex_lock(&queueLock);
    requestQueue[inputIndex] = *req;
    inputIndex = (inputIndex + 1) % MAX_SAVED_REQUESTS;
    pthread_mutex_unlock(&queueLock);
    if (logSyncMech(logfd, 0, SYNC_OP_MUTEX_UNLOCK, SYNC_ROLE_PRODUCER, req->value.header.pid) < 0)
    {
        fprintf(stderr, "Error writing to log. Moving on...\n");
    }
}

void getRequest(tlv_request_t *req)
{
    if (logSyncMech(logfd, getOfficeId(), SYNC_OP_MUTEX_LOCK, SYNC_ROLE_CONSUMER, req->value.header.pid) < 0)
    {
        fprintf(stderr, "Error writing to log. Moving on...\n");
    }
    pthread_mutex_lock(&queueLock);
    *req = requestQueue[outputIndex];
    outputIndex = (outputIndex + 1) % MAX_SAVED_REQUESTS;
    pthread_mutex_unlock(&queueLock);
    if (logSyncMech(logfd, getOfficeId(), SYNC_OP_MUTEX_UNLOCK, SYNC_ROLE_CONSUMER, req->value.header.pid) < 0)
    {
        fprintf(stderr, "Error writing to log. Moving on...\n");
    }
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

    if (logBankOfficeOpen(logfd, 0, offices[0].tid) < 0)
    {
        fprintf(stderr, "Error writing to log. Moving on...\n");
    }

    if (logAccountCreation(logfd, offices[accountsIndex].bankID, &account) < 0)
    {
        fprintf(stderr, "Error writing to log. Moving on...\n");
    }

    free(salt);
}

void createBankOffices()
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
        if (logBankOfficeOpen(logfd, offices[i].bankID, offices[i].tid) < 0)
        {
            fprintf(stderr, "Error writing to log. Moving on...\n");
        }
    }
}

int getOfficeId()
{
    pthread_t tid = pthread_self();
    int i;
    for (i = 0; i <= numThreads; i++)
    {
        if (offices[i].tid == tid)
            return offices[i].bankID;
    }

    printf("\nNOT SUPPOSED TO HAPPEN\n");
    return -1;
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

    if (logSyncMech(logfd, getOfficeId(), SYNC_OP_MUTEX_LOCK, SYNC_ROLE_ACCOUNT, value.header.pid) < 0)
    {
        fprintf(stderr, "Error writing to log. Moving on...\n");
    }
    pthread_mutex_lock(&accountLocks[accountsIndex]);
    if (logSyncDelay(logfd, getOfficeId(), bankAccounts[accountsIndex].account_id, value.header.op_delay_ms) < 0)
    {
        fprintf(stderr, "Error writing to log. Moving on...\n");
    }
    usleep(value.header.op_delay_ms * 1000);
    bankAccounts[accountsIndex] = account;
    if (logAccountCreation(logfd, offices[accountsIndex].bankID, &account) < 0)
    {
        fprintf(stderr, "Error writing to log. Moving on...\n");
    }
    pthread_mutex_unlock(&accountLocks[accountsIndex]);
    if (logSyncMech(logfd, getOfficeId(), SYNC_OP_MUTEX_UNLOCK, SYNC_ROLE_ACCOUNT, value.header.pid) < 0)
    {
        fprintf(stderr, "Error writing to log. Moving on...\n");
    }

    sHeader->ret_code = RC_OK;

    free(salt);
}

void checkBalance(req_header_t header, rep_header_t *sHeader, rep_balance_t *sBalance)
{
    sHeader->account_id = header.account_id;
    int index = checkLoginAccount(header);
    printf("loginCheck %d, index \n", index);

    sHeader->ret_code = RC_OK;
    if (logSyncMech(logfd, getOfficeId(), SYNC_OP_MUTEX_LOCK, SYNC_ROLE_ACCOUNT, header.pid) < 0)
    {
        fprintf(stderr, "Error writing to log. Moving on...\n");
    }
    pthread_mutex_lock(&accountLocks[index]);
    if (logSyncDelay(logfd, getOfficeId(), bankAccounts[index].account_id, header.op_delay_ms) < 0)
    {
        fprintf(stderr, "Error writing to log. Moving on...\n");
    }
    usleep(header.op_delay_ms * 1000);
    printf("account ballance - %d", bankAccounts[index].balance);
    fflush(stdout);
    sBalance->balance = bankAccounts[index].balance;
    pthread_mutex_unlock(&accountLocks[index]);
    if (logSyncMech(logfd, getOfficeId(), SYNC_OP_MUTEX_UNLOCK, SYNC_ROLE_ACCOUNT, header.pid) < 0)
    {
        fprintf(stderr, "Error writing to log. Moving on...\n");
    }
}

int isValidTransfer(req_value_t value, int index, rep_header_t *sHeader, rep_transfer_t *sTransfer)
{
    if (value.header.account_id == value.transfer.account_id)
    {
        sHeader->ret_code = RC_SAME_ID;
        if (logSyncMech(logfd, getOfficeId(), SYNC_OP_MUTEX_LOCK, SYNC_ROLE_ACCOUNT, value.header.pid) < 0)
        {
            fprintf(stderr, "Error writing to log. Moving on...\n");
        }
        pthread_mutex_lock(&accountLocks[index]);
        sTransfer->balance = bankAccounts[index].balance;
        pthread_mutex_unlock(&accountLocks[index]);
        if (logSyncMech(logfd, getOfficeId(), SYNC_OP_MUTEX_UNLOCK, SYNC_ROLE_ACCOUNT, value.header.pid) < 0)
        {
            fprintf(stderr, "Error writing to log. Moving on...\n");
        }
        return 1;
    }

    if (logSyncMech(logfd, getOfficeId(), SYNC_OP_MUTEX_LOCK, SYNC_ROLE_ACCOUNT, value.header.pid) < 0)
    {
        fprintf(stderr, "Error writing to log. Moving on...\n");
    }
    pthread_mutex_lock(&accountLocks[index]);
    if ((int)(bankAccounts[index].balance - value.transfer.amount) < (int)MIN_BALANCE)
    {
        sHeader->ret_code = RC_NO_FUNDS;
        sTransfer->balance = bankAccounts[index].balance;
        pthread_mutex_unlock(&accountLocks[index]);
        if (logSyncMech(logfd, getOfficeId(), SYNC_OP_MUTEX_UNLOCK, SYNC_ROLE_ACCOUNT, value.header.pid) < 0)
        {
            fprintf(stderr, "Error writing to log. Moving on...\n");
        }
        return 1;
    }
    pthread_mutex_unlock(&accountLocks[index]);
    if (logSyncMech(logfd, getOfficeId(), SYNC_OP_MUTEX_UNLOCK, SYNC_ROLE_ACCOUNT, value.header.pid) < 0)
    {
        fprintf(stderr, "Error writing to log. Moving on...\n");
    }

    return 0;
}

int finishTransfer(req_value_t value, int source, int dest, rep_header_t *sHeader, rep_transfer_t *sTransfer)
{
    if (value.transfer.account_id == bankAccounts[dest].account_id)
    {
        if (logSyncMech(logfd, getOfficeId(), SYNC_OP_MUTEX_LOCK, SYNC_ROLE_ACCOUNT, value.header.pid) < 0)
        {
            fprintf(stderr, "Error writing to log. Moving on...\n");
        }
        pthread_mutex_lock(&accountLocks[dest]);
        if (bankAccounts[dest].balance + value.transfer.amount > MAX_BALANCE)
        {
            pthread_mutex_unlock(&accountLocks[dest]);
            if (logSyncMech(logfd, getOfficeId(), SYNC_OP_MUTEX_UNLOCK, SYNC_ROLE_ACCOUNT, value.header.pid) < 0)
            {
                fprintf(stderr, "Error writing to log. Moving on...\n");
            }
            sHeader->ret_code = RC_TOO_HIGH;
            if (logSyncMech(logfd, getOfficeId(), SYNC_OP_MUTEX_LOCK, SYNC_ROLE_ACCOUNT, value.header.pid) < 0)
            {
                fprintf(stderr, "Error writing to log. Moving on...\n");
            }
            pthread_mutex_lock(&accountLocks[source]);
            sTransfer->balance = bankAccounts[source].balance;
            pthread_mutex_unlock(&accountLocks[source]);
            if (logSyncMech(logfd, getOfficeId(), SYNC_OP_MUTEX_UNLOCK, SYNC_ROLE_ACCOUNT, value.header.pid) < 0)
            {
                fprintf(stderr, "Error writing to log. Moving on...\n");
            }
            return 1;
        }
        pthread_mutex_unlock(&accountLocks[dest]);
        if (logSyncMech(logfd, getOfficeId(), SYNC_OP_MUTEX_UNLOCK, SYNC_ROLE_ACCOUNT, value.header.pid) < 0)
        {
            fprintf(stderr, "Error writing to log. Moving on...\n");
        }

        printf("previous amount 1 - %d \n", bankAccounts[source].balance);
        printf("previous amount 2 - %d \n", bankAccounts[dest].balance);
        if (logSyncMech(logfd, getOfficeId(), SYNC_OP_MUTEX_LOCK, SYNC_ROLE_ACCOUNT, value.header.pid) < 0)
        {
            fprintf(stderr, "Error writing to log. Moving on...\n");
        }
        pthread_mutex_lock(&accountLocks[dest]);
        if (logSyncDelay(logfd, getOfficeId(), bankAccounts[dest].account_id, value.header.op_delay_ms) < 0)
        {
            fprintf(stderr, "Error writing to log. Moving on...\n");
        }
        usleep(value.header.op_delay_ms * 1000);
        bankAccounts[dest].balance += value.transfer.amount;
        pthread_mutex_unlock(&accountLocks[dest]);
        if (logSyncMech(logfd, getOfficeId(), SYNC_OP_MUTEX_UNLOCK, SYNC_ROLE_ACCOUNT, value.header.pid) < 0)
        {
            fprintf(stderr, "Error writing to log. Moving on...\n");
        }
        if (logSyncMech(logfd, getOfficeId(), SYNC_OP_MUTEX_LOCK, SYNC_ROLE_ACCOUNT, value.header.pid) < 0)
        {
            fprintf(stderr, "Error writing to log. Moving on...\n");
        }
        pthread_mutex_lock(&accountLocks[source]);
        if (logSyncDelay(logfd, getOfficeId(), bankAccounts[source].account_id, value.header.op_delay_ms) < 0)
        {
            fprintf(stderr, "Error writing to log. Moving on...\n");
        }
        usleep(value.header.op_delay_ms * 1000);
        bankAccounts[source].balance -= value.transfer.amount;
        pthread_mutex_unlock(&accountLocks[source]);
        if (logSyncMech(logfd, getOfficeId(), SYNC_OP_MUTEX_UNLOCK, SYNC_ROLE_ACCOUNT, value.header.pid) < 0)
        {
            fprintf(stderr, "Error writing to log. Moving on...\n");
        }
        printf("new amount 1 - %d \n", bankAccounts[source].balance);
        printf("new amount 2 - %d \n", bankAccounts[dest].balance);
        fflush(stdout);

        sHeader->ret_code = RC_OK;
        if (logSyncMech(logfd, getOfficeId(), SYNC_OP_MUTEX_LOCK, SYNC_ROLE_ACCOUNT, value.header.pid) < 0)
        {
            fprintf(stderr, "Error writing to log. Moving on...\n");
        }
        pthread_mutex_lock(&accountLocks[source]);
        sTransfer->balance = bankAccounts[source].balance;
        pthread_mutex_unlock(&accountLocks[source]);
        if (logSyncMech(logfd, getOfficeId(), SYNC_OP_MUTEX_UNLOCK, SYNC_ROLE_ACCOUNT, value.header.pid) < 0)
        {
            fprintf(stderr, "Error writing to log. Moving on...\n");
        }
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
    if (logSyncMech(logfd, getOfficeId(), SYNC_OP_MUTEX_LOCK, SYNC_ROLE_ACCOUNT, value.header.pid) < 0)
    {
        fprintf(stderr, "Error writing to log. Moving on...\n");
    }
    pthread_mutex_lock(&accountLocks[index]);
    sTransfer->balance = bankAccounts[index].balance;
    pthread_mutex_unlock(&accountLocks[index]);
    if (logSyncMech(logfd, getOfficeId(), SYNC_OP_MUTEX_UNLOCK, SYNC_ROLE_ACCOUNT, value.header.pid) < 0)
    {
        fprintf(stderr, "Error writing to log. Moving on...\n");
    }
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
        if (logDelay(logfd, getOfficeId(), tlv.value.header.op_delay_ms) < 0)
        {
            fprintf(stderr, "Error writing to log. Moving on...\n");
        }
        usleep(tlv.value.header.op_delay_ms * 1000);
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

int processRequestReply(tlv_request_t tlv)
{
    rep_header_t sHeader;
    rep_balance_t sBalance;
    rep_transfer_t sTransfer;
    rep_value_t sValue;
    tlv_reply_t *rTlv = (tlv_reply_t *)malloc(sizeof(struct tlv_reply));

    if (logSyncMech(logfd, getOfficeId(), SYNC_OP_MUTEX_LOCK, SYNC_ROLE_CONSUMER, tlv.value.header.pid) < 0)
    {
        fprintf(stderr, "Error writing to log. Moving on...\n");
    }
    pthread_mutex_lock(&activeLock);
    activeThreads++;
    pthread_mutex_unlock(&activeLock);
    if (logSyncMech(logfd, getOfficeId(), SYNC_OP_MUTEX_UNLOCK, SYNC_ROLE_CONSUMER, tlv.value.header.pid) < 0)
    {
        fprintf(stderr, "Error writing to log. Moving on...\n");
    }

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

    int userFIFO = open(pathFIFO, O_WRONLY | O_NONBLOCK);

    if (userFIFO == -1)
    {
        rTlv->value.header.ret_code = RC_USR_DOWN;
        if (logReply(logfd, getOfficeId(), rTlv) < 0)
        {
        fprintf(stderr, "Error writing to log. Moving on...\n");
        }
        return 1;
    }

    //TODO: remove following 3 lines
    printf("\nid - %d\n", rTlv->value.header.account_id);
    printf("retorno - %d\n", rTlv->value.header.ret_code);
    fflush(stdout);

    if (logReply(logfd, getOfficeId(), rTlv) < 0)
    {
        fprintf(stderr, "Error writing to log. Moving on...\n");
    }
    write(userFIFO, rTlv, sizeof(*rTlv));
    close(userFIFO);

    if (logSyncMech(logfd, getOfficeId(), SYNC_OP_MUTEX_LOCK, SYNC_ROLE_CONSUMER, tlv.value.header.pid) < 0)
    {
        fprintf(stderr, "Error writing to log. Moving on...\n");
    }
    pthread_mutex_lock(&activeLock);
    activeThreads--;
    pthread_mutex_unlock(&activeLock);
    if (logSyncMech(logfd, getOfficeId(), SYNC_OP_MUTEX_UNLOCK, SYNC_ROLE_CONSUMER, tlv.value.header.pid) < 0)
    {
        fprintf(stderr, "Error writing to log. Moving on...\n");
    }

    return 0;
}

void *consumeRequest(void *arg)
{
    tlv_request_t tlv;

    while (1)
    {
        if (logSyncMech(logfd, getOfficeId(), SYNC_OP_MUTEX_LOCK, SYNC_ROLE_CONSUMER, tlv.value.header.pid) < 0)
        {
            fprintf(stderr, "Error writing to log. Moving on...\n");
        }
        pthread_mutex_lock(&requestsLock);
        if (shutdown && requests <= 0)
        {
            pthread_cond_broadcast(&requestsCond);
            if (logSyncMech(logfd, getOfficeId(), SYNC_OP_COND_SIGNAL, SYNC_ROLE_CONSUMER, tlv.value.header.pid) < 0)
            {
                fprintf(stderr, "Error writing to log. Moving on...\n");
            }
            pthread_mutex_unlock(&requestsLock);
            if (logSyncMech(logfd, getOfficeId(), SYNC_OP_MUTEX_UNLOCK, SYNC_ROLE_CONSUMER, tlv.value.header.pid) < 0)
            {
                fprintf(stderr, "Error writing to log. Moving on...\n");
            }
            break;
        }

        while (requests <= 0)
        {
            if (logSyncMech(logfd, getOfficeId(), SYNC_OP_COND_WAIT, SYNC_ROLE_CONSUMER, tlv.value.header.pid) < 0)
            {
                fprintf(stderr, "Error writing to log. Moving on...\n");
            }
            pthread_cond_wait(&requestsCond, &requestsLock);
            if (shutdown)
            {
                printf("\nThread Finished!\n");
                pthread_mutex_unlock(&requestsLock);
                if (logSyncMech(logfd, getOfficeId(), SYNC_OP_MUTEX_UNLOCK, SYNC_ROLE_CONSUMER, tlv.value.header.pid) < 0)
                {
                    fprintf(stderr, "Error writing to log. Moving on...\n");
                }
                if (logBankOfficeClose(logfd, getOfficeId(), pthread_self()) < 0)
                {
                    fprintf(stderr, "Error writing to log. Moving on...\n");
                }
                pthread_exit(NULL);
            }
        }
        requests--;
        pthread_mutex_unlock(&requestsLock);
        if (logSyncMech(logfd, getOfficeId(), SYNC_OP_MUTEX_UNLOCK, SYNC_ROLE_CONSUMER, tlv.value.header.pid) < 0)
        {
            fprintf(stderr, "Error writing to log. Moving on...\n");
        }

        getRequest(&tlv);
        if (logRequest(logfd, getOfficeId(), &tlv) < 0)
        {
            fprintf(stderr, "Error writing to log. Moving on...\n");
        }

        if (processRequestReply(tlv))
            continue;

        if (logSyncMech(logfd, getOfficeId(), SYNC_OP_MUTEX_LOCK, SYNC_ROLE_CONSUMER, tlv.value.header.pid) < 0)
        {
            fprintf(stderr, "Error writing to log. Moving on...\n");
        }
        pthread_mutex_lock(&slotsLock);
        slots++;
        pthread_cond_signal(&slotsCond);
        if (logSyncMech(logfd, getOfficeId(), SYNC_OP_COND_SIGNAL, SYNC_ROLE_CONSUMER, tlv.value.header.pid) < 0)
        {
            fprintf(stderr, "Error writing to log. Moving on...\n");
        }
        pthread_mutex_unlock(&slotsLock);
        if (logSyncMech(logfd, getOfficeId(), SYNC_OP_MUTEX_UNLOCK, SYNC_ROLE_CONSUMER, tlv.value.header.pid) < 0)
        {
            fprintf(stderr, "Error writing to log. Moving on...\n");
        }
    }

    printf("\nThread Finished!\n");
    if (logBankOfficeClose(logfd, getOfficeId(), pthread_self()) < 0)
    {
        fprintf(stderr, "Error writing to log. Moving on...\n");
    }
    pthread_exit(NULL);
}

void produceRequest(tlv_request_t tlv)
{
    if (logSyncMech(logfd, 0, SYNC_OP_MUTEX_LOCK, SYNC_ROLE_PRODUCER, tlv.value.header.pid) < 0)
    {
        fprintf(stderr, "Error writing to log. Moving on...\n");
    }
    pthread_mutex_lock(&slotsLock);
    while (slots <= 0)
    {
        if (logSyncMech(logfd, 0, SYNC_OP_COND_WAIT, SYNC_ROLE_PRODUCER, tlv.value.header.pid) < 0)
        {
            fprintf(stderr, "Error writing to log. Moving on...\n");
        }
        pthread_cond_wait(&slotsCond, &slotsLock);
    }
    slots--;
    pthread_mutex_unlock(&slotsLock);
    if (logSyncMech(logfd, 0, SYNC_OP_MUTEX_UNLOCK, SYNC_ROLE_PRODUCER, tlv.value.header.pid) < 0)
    {
        fprintf(stderr, "Error writing to log. Moving on...\n");
    }

    insertRequest(&tlv);
    if (logRequest(logfd, 0, &tlv) < 0)
    {
        fprintf(stderr, "Error writing to log. Moving on...\n");
    }

    if (logSyncMech(logfd, 0, SYNC_OP_MUTEX_LOCK, SYNC_ROLE_PRODUCER, tlv.value.header.pid) < 0)
    {
        fprintf(stderr, "Error writing to log. Moving on...\n");
    }
    pthread_mutex_lock(&requestsLock);
    requests++;
    pthread_cond_signal(&requestsCond);
    if (logSyncMech(logfd, 0, SYNC_OP_COND_SIGNAL, SYNC_ROLE_PRODUCER, tlv.value.header.pid) < 0)
    {
        fprintf(stderr, "Error writing to log. Moving on...\n");
    }
    pthread_mutex_unlock(&requestsLock);
    if (logSyncMech(logfd, 0, SYNC_OP_MUTEX_UNLOCK, SYNC_ROLE_PRODUCER, tlv.value.header.pid) < 0)
    {
        fprintf(stderr, "Error writing to log. Moving on...\n");
    }
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

    numThreads = atoi(argv[1]);
    logfd = open(SERVER_LOGFILE, O_WRONLY | O_CREAT | O_APPEND, 0664);

    validatePassword(argv[2]);

    createAdmin(argv[2]);

    createBankOffices();

    mkfifo(SERVER_FIFO_PATH, 0666);

    bankCycle();

    unlink(SERVER_FIFO_PATH);

    if (logBankOfficeClose(logfd, 0, pthread_self()) < 0)
    {
        fprintf(stderr, "Error writing to log. Moving on...\n");
    }
    pthread_exit(NULL);
}