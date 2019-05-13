#pragma once

#include "types.h"
#include "user.h"

/**
 * @brief Adds the admin to the position 0 of the array bankAccounts
 * 
 * @param char *pass Admin password
 */
void createAdmin(char *pass);

/**
 * @brief Verifies if the pair id and password already exists
 * 
 * @param req_header_t struct header
 * @return Int If the user already exits the index in the account array, otherwise -1
 */
int checkLoginAccount(req_header_t header);

/**
 * @brief Verifies if the ID already exists
 * 
 * @param uint32_t id to be compared with
 * @return Int If the ID already exits the ID itself, otherwise -1
 */
int searchID(uint32_t id);

/**
 * @brief Adds a new account to the array bankAccounts
 * 
 * @param req_create_account_t newAccount Struct with the new account information
 * @param rep_header_t* sHeader Struct used for the reply
 */
void createNewAccount(req_create_account_t newAccount, rep_header_t *sHeader);

/**
 * @brief Finds the balance of the user account
 * 
 * @param req_header_t header Struct with the account information
 * @Param rep_header_t* sHeader Struct used for the reply
 * @Param rep_balance_t *sBalance Struct used for balance op the reply
 */
void checkBalance(req_header_t header, rep_header_t *sHeader, rep_balance_t *sBalance);

/**
 * @brief Finds the balance of the user account
 * 
 * @param req_value_t value Struct with the account information
 * @param int index Index of the user in the bankAccounts array
 * @param rep_header_t* sHeader Struct used for the reply
 * @param rep_transfer_t* sTransfer Struct used for transfer op reply
 * @return int If the tranfer is invalid 1, otherwise -1
 */
int transferChecks(req_value_t value, int index, rep_header_t *sHeader, rep_transfer_t *sTransfer);

/**
 * @brief Does the trnsfer it is valid
 * 
 * @param req_value_t value Struct with the account information
 * @param int index Index of the user in the bankAccounts array
 * @param int i index of the account where the money will be transfered
 * @param rep_header_t* sHeader Struct used for the reply
 * @param rep_transfer_t* sTransfer Struct used for transfer op reply
 * @return int returns 1 once the id matches the id from the array
 */
int finishTransfer(req_value_t value, int index, int i, rep_header_t *sHeader, rep_transfer_t *sTransfer);

/**
 * @brief Transfers money from one account to another
 * 
 * @param req_value_t value Struct with the account information
 * @param rep_header_t* sHeader Struct used for the reply
 * @param rep_transfer_t* sTransfer Struct used for transfer op reply
 */
void bankTransfer(req_value_t value, rep_header_t *sHeader, rep_transfer_t *sTransfer);

/**
 * @brief Verifies if the tlv accountID has privileges for a given tvl type operation
 * 
 * @param tlv_request_t tlv Struct with the account information
 * @param rep_header_t *sHeader Struct used for the reply
 * @param rep_header_t *sTransfer Struct used for the reply
 * @return int 0 f the user matches the operation, 1 otherwise
 */
int opUser(tlv_request_t tlv, rep_header_t *sHeader, rep_transfer_t *sTransfer);

/**
 * @brief Treats the requests from the users
 * 
 * @param tlv_request_t tlv Struct with the account information
 * @param rep_header_t *sHeader Struct used for the reply
 * @param rep_balance_t *sBalance Struct used for the reply
 * @param rep_transfer_t *sTransfer Struct used for the reply
 * @param rep_value_t *sValue Struct used for the reply
 * @return int 1 if there was a shutdown request
 */
int requestHandler(tlv_request_t tlv, rep_header_t *sHeader, rep_balance_t *sBalance, rep_transfer_t *sTransfer, rep_value_t *sValue);

/**
 * @brief Generates the user FIFO path based on its PID
 * 
 * @param tlv_request_t tlv Struct with the account information
 * @return char* FIFO path
 */
char *getFIFOName(tlv_request_t tlv);

void bankCycle();