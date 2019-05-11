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
#include <sys/stat.h>

bank_account_t bankAccounts[MAX_BANK_ACCOUNTS];
int arrayIndex = 0;

char* generateSalt()
{
    char hex[16] = "0123456789abcdef";
    char* salt = malloc(SALT_LEN+1);
    /* Seed number for rand() */

    for (int i = 0; i < SALT_LEN+1; i++)
    {
        salt[i] = hex[rand() % 16];
    }
    salt[SALT_LEN]='\0';
    return salt;
} 

char* generateHash(char* salt, char* password)
{
    FILE* fpout;
    char* hash = malloc(sizeof(HASH_LEN));
    char* cmd = malloc(sizeof("echo -n | sha256sum") + MAX_PASSWORD_LEN + SALT_LEN + 1);
    sprintf(cmd,"echo -n %s%s | sha256sum",password, salt);
    fpout = popen(cmd, "r");
    fgets(hash, HASH_LEN, fpout);

    return hash;
}

void createAdmin(int argc, char *argv[], char* pass)
{
    bank_account_t account;
    char* salt = malloc(SALT_LEN);
    strcpy(salt, generateSalt());
    strcpy(account.hash, generateHash(salt, pass));
    strcpy(account.salt, salt);
    account.balance = 0;
    account.account_id = ADMIN_ACCOUNT_ID;
    bankAccounts[0] = account;
}

int checkLoginAccount(req_header_t header)
{
    char* hash = malloc(HASH_LEN);

    for(int i=0; i<=arrayIndex;i++)
    {
        
        hash = generateHash(bankAccounts[i].salt, header.password);

        if(header.account_id == bankAccounts[i].account_id && !strcmp(hash, bankAccounts[i].hash))
        {
            return i;
        }
    }

    return -1;
}


void createNewAccount(req_create_account_t createAccount)
{
    arrayIndex++;
    bank_account_t account;
    char* salt = malloc(SALT_LEN);
    strcpy(salt, generateSalt());
    strcpy(account.hash, generateHash(salt, createAccount.password));
    strcpy(account.salt, salt);
    account.balance = createAccount.balance;
    account.account_id = createAccount.account_id;
    printf("array index e id- %d %d \n",arrayIndex, account.account_id);
    fflush(stdout);
    bankAccounts[arrayIndex] = account;
    
}


int checkBalance(req_header_t header)
{
   int index = checkLoginAccount(header);
    printf("loginCheck %d, index \n", index);

   if(index > -1)
   {
       printf("account ballance - %d", bankAccounts[index].balance);
       fflush(stdout);
       return bankAccounts[index].balance;
   }

    return 0;
}

int bankTransfer(req_value_t value)
{
    int index = checkLoginAccount(value.header);
    printf("loginCheck %d, index \n", index);
    if(index > -1)
    {
        if(bankAccounts[index].balance < value.transfer.amount)
        {
            return 0;
        }

        for(int i=0; i<=arrayIndex;i++)
        {
            if(value.transfer.account_id == bankAccounts[i].account_id)
            {
                printf("previous amount 1 - %d \n", bankAccounts[index].balance);
                printf("previous amount 2 - %d \n", bankAccounts[i].balance);
                bankAccounts[i].balance += value.transfer.amount;
                bankAccounts[index].balance -= value.transfer.amount;
                printf("new amount 1 - %d \n", bankAccounts[index].balance);
                printf("new amount 2 - %d \n", bankAccounts[i].balance);
                fflush(stdout);
                return 1;
            }
        }
    }

    return 0;
}

int serverDown(req_header_t header)
{
    int index = checkLoginAccount(header);
    if(index == 0)
    {
        return 1;
    }

    return 0;
}

int RequestHandler(tlv_request_t tlv)
{
    printf("op: %d \n", tlv.type);
    switch (tlv.type)
    {
    case OP_CREATE_ACCOUNT:
        createNewAccount(tlv.value.create);
        break;

    case OP_BALANCE:
        checkBalance(tlv.value.header);
        break;
    
    case OP_TRANSFER:
        bankTransfer(tlv.value);
        break;

    case OP_SHUTDOWN:
        return -1;
        break;

    default:
        break;
    }

    return 0;
}

int main(int argc, char *argv[])
{

    if (argc != 3)
    {
        printf("Usage: %s nThreads password \n", argv[0]);
        exit(1);
    }

    char pass[MAX_PASSWORD_LEN];
    strcpy(pass, argv[argc-1]);

    createAdmin(argc, argv, pass);

    tlv_request_t tlv;

    mkfifo(SERVER_FIFO_PATH, 0666);

    while(1)
    {  
        int fifo = open(SERVER_FIFO_PATH, O_RDONLY); 
        int r;

        if((r=read(fifo, &tlv, sizeof(tlv_request_t))) >= 0)
        {   
            if(RequestHandler(tlv) == -1)
            {
                if(serverDown(tlv.value.header))
                {
                    close(fifo);
                    break;
                }   
            }
        }

        close(fifo);
    }
        unlink(SERVER_FIFO_PATH);  

    return 0;
}