#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include <pthread.h>
#include <unistd.h>

#include "constants.h"
#include "types.h"

/**
 * @brief Log request message into file.
 * 
 * @param fd The file descriptor.
 * @param id The id to be printed: PID for users, bank office/main thread ID for server.
 * @param request The request message.
 * @return int Number of printed characters or a negative value in case of error.
 */
int logRequest(int fd, int id, const tlv_request_t *request);

/**
 * @brief Log reply message into file.
 * 
 * @param fd The file descriptor.
 * @param id The id to be printed: PID for users, bank office/main thread ID for server.
 * @param reply The reply message.
 * @return int Number of printed characters or a negative value in case of error.
 */
int logReply(int fd, int id, const tlv_reply_t *request);

/**
 * @brief Log a bank office opening.
 * 
 * @param fd The file descriptor.
 * @param id The bank office/main thread ID.
 * @param tid The thread ID as defined by POSIX.
 * @return int Number of printed characters or a negative value in case of error.
 */
int logBankOfficeOpen(int fd, int id, pthread_t tid);

/**
 * @brief Log a bank office closing.
 * 
 * @param fd The file descriptor.
 * @param id The bank office/main thread ID.
 * @param tid The thread ID as defined by POSIX.
 * @return int Number of printed characters or a negative value in case of error.
 */
int logBankOfficeClose(int fd, int id, pthread_t tid);

/**
 * @brief Log account creation (additional information).
 * 
 * @param fd The file descriptor.
 * @param id The bank office/main thread ID.
 * @param account The account information.
 * @return int Number of printed characters or a negative value in case of error.
 */
int logAccountCreation(int fd, int id, const bank_account_t *account);

/**
 * @brief Log the usage of a synchronization mechanism function (other than semaphores).
 * 
 * @param fd The file descriptor.
 * @param id The bank office/main thread ID.
 * @param smo The function being used.
 * @param role Producer, consumer or account.
 * @param sid The PID included in the request or the account ID.
 * @return int Number of printed characters or a negative value in case of error.
 */
int logSyncMech(int fd, int id, sync_mech_op_t smo, sync_role_t role, int sid);

/**
 * @brief Log the usage of a semaphore's synchronization mechanism function.
 * 
 * @param fd The file descriptor.
 * @param id The bank office/main thread ID.
 * @param smo The function being used.
 * @param role Producer, consumer or account.
 * @param sid The PID included in the request or the account ID.
 * @param val The current value of the semaphore.
 * @return int Number of printed characters or a negative value in case of error.
 */
int logSyncMechSem(int fd, int id, sync_mech_op_t smo, sync_role_t role, int sid, int val);

/**
 * @brief Log the delay introduced (server shutdown only).
 * 
 * @param fd The file descriptor.
 * @param id The bank office/main thread ID.
 * @param delay_ms The delay in milliseconds.
 * @return int Number of printed characters or a negative value in case of error.
 */
int logDelay(int fd, int id, uint32_t delay_ms);

/**
 * @brief Log the delay introduced immediately after entering the critical section of an account.
 * 
 * @param fd The file descriptor.
 * @param id The bank office/main thread ID.
 * @param sid The account ID.
 * @param delay_ms The delay in milliseconds.
 * @return int Number of printed characters or a negative value in case of error.
 */
int logSyncDelay(int fd, int id, int sid, uint32_t delay_ms);
