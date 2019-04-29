#pragma once

#include <stdint.h>

#include <unistd.h>

#include "constants.h"

typedef enum op_type {
  OP_CREATE_ACCOUNT, // create a bank account (only available to admin)
  OP_BALANCE,        // check account balance (only available to clients)
  OP_TRANSFER,       // make a transfer (only available to clients)
  OP_SHUTDOWN,       // shut down home banking service (only available to admin)
  __OP_MAX_NUMBER    // enables to determine how many operations are defined
} op_type_t;

// if there is more than one error, the first within this list must be returned
typedef enum ret_code {
  RC_OK,           // operation was performed successfully
  RC_SRV_DOWN,     // the server is currently unavailable (down)
  RC_SRV_TIMEOUT,  // the request sent to server timed out
  RC_USR_DOWN,     // unable to send reply message to user
  RC_LOGIN_FAIL,   // the account id / password pair is not valid
  RC_OP_NALLOW,    // the user is not allowed to request such operation
  RC_ID_IN_USE,    // the account id is already in use
  RC_ID_NOT_FOUND, // there is no account with that id
  RC_SAME_ID,      // the source and destination accounts are the same
  RC_NO_FUNDS,     // final balance would be too low (less than MIN_BALANCE)
  RC_TOO_HIGH,     // final balance would be to high (greater than MAX_BALANCE)
  RC_OTHER,        // any other error code that is not defined
  __RC_MAX_NUMBER  // enables to determine how many return codes are defined
} ret_code_t;

typedef enum sync_mech_op_type {
  SYNC_OP_MUTEX_LOCK,    // pthread_mutex_lock() call
  SYNC_OP_MUTEX_UNLOCK,  // pthread_mutex_unlock() call
  SYNC_OP_MUTEX_TRYLOCK, // pthread_mutex_trylock() call
  SYNC_OP_COND_SIGNAL,   // pthread_cond_signal() call
  SYNC_OP_COND_WAIT,     // pthread_cond_wait() call
  SYNC_OP_SEM_INIT,      // pthread_sem_init() call
  SYNC_OP_SEM_POST,      // pthread_sem_post() call
  SYNC_OP_SEM_WAIT,      // pthread_sem_wait() call
  __SYNC_OP_MAX_NUMBER   // enables to determine how many sync operations are defined
} sync_mech_op_t;

typedef enum sync_mech_role {
  SYNC_ROLE_PRODUCER,    // producer (main thread)
  SYNC_ROLE_CONSUMER,    // consumer (bank office threads)
  SYNC_ROLE_ACCOUNT,     // account related
  __SYNC_ROLE_MAX_NUMBER // enables to determine how many sync roles are defined
} sync_role_t;

/**
 * @brief Bank account information.
 */
typedef struct bank_account {
  uint32_t account_id;
  char hash[HASH_LEN + 1];
  char salt[SALT_LEN + 1];
  uint32_t balance;
} bank_account_t;

//
//
// Request message related types.
//
//

/**
 * @brief Request header fields (always present).
 */
typedef struct req_header {
  pid_t pid;
  uint32_t account_id;
  char password[MAX_PASSWORD_LEN + 1];
  uint32_t op_delay_ms;
} __attribute__((packed)) req_header_t;

/**
 * @brief Required arguments for account creation request.
 */
typedef struct req_create_account {
  uint32_t account_id;
  uint32_t balance;
  char password[MAX_PASSWORD_LEN + 1];
} __attribute__((packed)) req_create_account_t;

/**
 * @brief Required arguments for wire transfer request.
 */
typedef struct req_transfer {
  uint32_t account_id;
  uint32_t amount;
} __attribute__((packed)) req_transfer_t;

/**
 * @brief Request message: header and arguments, if any.
 * 
 * This struct eases request message (de)serialization.
 */
typedef struct req_value {
  req_header_t header;
  union {
    req_create_account_t create;
    req_transfer_t transfer;
  };
} __attribute__((packed)) req_value_t;

/**
 * @brief Full request message in TLV format.
 */
typedef struct tlv_request {
  enum op_type type;
  uint32_t length;
  req_value_t value;
} __attribute__((packed)) tlv_request_t;

//
//
// Reply message related types.
//
//

/**
 * @brief Reply header fields (always present).
 */
typedef struct rep_header {
  uint32_t account_id;
  int ret_code;
} __attribute__((packed)) rep_header_t;

/**
 * @brief Reply message info regarding account balance check.
 */
typedef struct rep_balance {
  uint32_t balance;
} __attribute__((packed)) rep_balance_t;

/**
 * @brief Reply message info regarding wire transfer.
 */
typedef struct rep_transfer {
  uint32_t balance;
} __attribute__((packed)) rep_transfer_t;

/**
 * @brief Reply message info regarding service shutdown.
 */
typedef struct rep_shutdown {
  uint32_t active_offices;
} __attribute__((packed)) rep_shutdown_t;

/**
 * @brief Reply message: header and any returned info.
 * 
 * This struct eases reply message (de)serialization.
 */
typedef struct rep_value {
  rep_header_t header;
  union {
    rep_balance_t balance;
    rep_transfer_t transfer;
    rep_shutdown_t shutdown;
  };
} __attribute__((packed)) rep_value_t;

/**
 * @brief Full reply message in TLV format.
 */
typedef struct tlv_reply {
  enum op_type type;
  uint32_t length;
  rep_value_t value;
} __attribute__((packed)) tlv_reply_t;
