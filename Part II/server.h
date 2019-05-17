#pragma once

#include "types.h"
#include "user.h"

/**
 * @brief Push a request into the request queue.
 * 
 * @param req The request's memory address.
 */
void insertRequest(const tlv_request_t *req);

/**
 * @brief Pop a request from the request queue.
 * 
 * @param req The request's memory address.
 */
void getRequest(tlv_request_t *req);

/**
 * @brief Creates Admin account.
 * 
 * @param pass Admin's password.
 */
void createAdmin(char *pass);

/**
 * @brief Initializes every bank office thread. It's id and tid are saved in offices array.
 */
void createBankOffices();

/**
 * @brief Get the current thread bank id (For log purposes).
 * 
 * @return The id if the thread's tid exists in offices array, -1 otherwise.
 */
int getOfficeId();

/**
 * @brief Verifies if the pair id and password already exists.
 * 
 * @param header of the request's value.
 * @return If the log in is correct the user's account index is returned, otherwise -1 is returned.
 */
int checkLoginAccount(req_header_t header);

/**
 * @brief Verifies if the ID already exists.
 * 
 * @param id ID to be compared with.
 * @return If the ID exists, then it is returned. Otherwise, -1 is returned.
 */
int searchID(uint32_t id);

/**
 * @brief Adds a new account to the bankAccounts array.
 * 
 * @param newAccount Struct with the new account information.
 * @param sHeader Struct used for the reply.
 */
void createNewAccount(req_value_t value, rep_header_t *sHeader);

/**
 * @brief Finds the balance of the user account.
 * 
 * @param header Struct with the account information.
 * @Param sHeader Struct used for the reply.
 * @Param sBalance Struct used for balance operation reply.
 */
void checkBalance(req_header_t header, rep_header_t *sHeader, rep_balance_t *sBalance);

/**
 * @brief Checks if the requested transfer is valid.
 * 
 * @param value Struct with the account information.
 * @param index User's account index.
 * @param sHeader Struct used for the reply.
 * @param sTransfer Struct used for transfer operation reply.
 * @return If the tranfer is invalid 1, otherwise -1.
 */
int isValidTransfer(req_value_t value, int index, rep_header_t *sHeader, rep_transfer_t *sTransfer);

/**
 * @brief Finishes the transfer operation.
 * 
 * @param value Struct with the account information.
 * @param source User's account index (the source of the money).
 * @param dest Destination's account possible index.
 * @param sHeader Struct used for the reply.
 * @param sTransfer Struct used for transfer operation reply.
 * @return If dest is the destination's id passed in the request, then 1 is returned. Otherwise 0 is returned.
 */
int finishTransfer(req_value_t value, int source, int dest, rep_header_t *sHeader, rep_transfer_t *sTransfer);

/**
 * @brief Transfers money from one account to another.
 * 
 * @param value Struct with the account information.
 * @param sHeader Struct used for the reply.
 * @param sTransfer Struct used for transfer operation reply.
 */
void bankTransfer(req_value_t value, rep_header_t *sHeader, rep_transfer_t *sTransfer);

/**
 * @brief Verifies if the user has permission to do what he requested.
 * 
 * @param tlv Struct with the request information.
 * @param sHeader Struct used for the reply.
 * @param sTransfer Struct used for the reply.
 * @return 0 if the user has permissions for the operation, 1 otherwise.
 */
int opUser(tlv_request_t tlv, rep_header_t *sHeader, rep_transfer_t *sTransfer);

/**
 * @brief Handles an user request.
 * 
 * @param tlv Struct with the request information
 * @param sHeader Struct used for the reply
 * @param sBalance Struct used for the reply
 * @param sTransfer Struct used for the reply
 * @param sValue Struct used for the reply
 * @return Returns -1 if there was a shutdown request and 0 otherwise.
 */
int requestHandler(tlv_request_t tlv, rep_header_t *sHeader, rep_balance_t *sBalance, rep_transfer_t *sTransfer, rep_value_t *sValue);

/**
 * @brief Processes an user's request and replies to it.
 * 
 * @param tlv Struct with the request information.
 * @return -1 if there was an error opening the user's FIFO.
 */
int processRequestReply(tlv_request_t tlv);

/**
 * @brief Consumer function. Gets a request from the queue, waiting when there are none, and processes it.
 *      This function is the one used by bank offices (threads).
 * 
 * @param arg Not used, has to be present to be a thread compatible function.
 */
void *consumeRequest(void *arg);

/**
 * @brief Producer function. Inserts a request into the queue, waiting when there are no slots available.
 * 
 * @param tlv The request to be inserted.
 */
void produceRequest(tlv_request_t tlv);

/**
 * @brief The main thread cycle. Responsible for reading and producing requests.
 */
void bankCycle();

/**
 * @brief Generates the user FIFO path based on the user's PID (for replying purposes).
 * 
 * @param tlv Struct with the request information (including user's PID).
 * @return User's FIFO path.
 */
char *getUserFifo(tlv_request_t tlv);