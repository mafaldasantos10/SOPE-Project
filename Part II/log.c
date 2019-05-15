#include "sope.h"

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <limits.h>
#include <pthread.h>
#include <sys/syscall.h>
#include <unistd.h>

static const char *OP_TYPE_STR[] = {
  [OP_CREATE_ACCOUNT] = "CREATE",
  [OP_BALANCE] = "BALANCE",
  [OP_TRANSFER] = "TRANSFER",
  [OP_SHUTDOWN] = "SHUTDOWN"};

static const char *RC_STR[] = {
  [RC_OK] = "OK",
  [RC_SRV_DOWN] = "SRV_DOWN",
  [RC_SRV_TIMEOUT] = "SRV_TIMEOUT",
  [RC_USR_DOWN] = "USR_DOWN",
  [RC_LOGIN_FAIL] = "LOGIN_FAIL",
  [RC_OP_NALLOW] = "OP_NALLOW",
  [RC_ID_IN_USE] = "ID_IN_USE",
  [RC_ID_NOT_FOUND] = "ID_NOT_FOUND",
  [RC_SAME_ID] = "SAME_ID",
  [RC_NO_FUNDS] = "NO_FUNDS",
  [RC_TOO_HIGH] = "TOO_HIGH",
  [RC_OTHER] = "OTHER"};

static const char *SYNC_MECH_STR[] = {
  [SYNC_OP_MUTEX_LOCK] = "MUTEX_LOCK",
  [SYNC_OP_MUTEX_TRYLOCK] = "MUTEX_TRYLOCK",
  [SYNC_OP_MUTEX_UNLOCK] = "MUTEX_UNLOCK",
  [SYNC_OP_COND_SIGNAL] = "COND_SIGNAL",
  [SYNC_OP_COND_WAIT] = "COND_WAIT",
  [SYNC_OP_SEM_INIT] = "SEM_INIT",
  [SYNC_OP_SEM_POST] = "SEM_POST",
  [SYNC_OP_SEM_WAIT] = "SEM_WAIT"};

static const char *SYNC_ROLE_STR[] = {
  [SYNC_ROLE_ACCOUNT] = "ACCOUNT",
  [SYNC_ROLE_CONSUMER] = "CONSUMER",
  [SYNC_ROLE_PRODUCER] = "PRODUCER"};

static int atomicPrintf(int fd, const char *format, ...);

static char *logBaseRequestInfo(int id, char *str, const tlv_request_t *request);

static char *logBaseReplyInfo(int id, char *str, const tlv_reply_t *reply);

static int logBankOfficeInfo(int fd, int id, pthread_t tid, bool open);

int logRequest(int fd, int id, const tlv_request_t *request) {
  if (!request)
    return -1;

  char buffer[PIPE_BUF];

  switch (request->type) {
    case OP_CREATE_ACCOUNT:
      return atomicPrintf(fd, "%s %0*d %*u€ \"%s\"\n",
                          logBaseRequestInfo(id, buffer, request),
                          WIDTH_ACCOUNT, request->value.create.account_id,
                          WIDTH_BALANCE, request->value.create.balance,
                          request->value.create.password);
    case OP_BALANCE: return atomicPrintf(fd, "%s\n", logBaseRequestInfo(id, buffer, request));
    case OP_TRANSFER:
      return atomicPrintf(fd, "%s %0*d %*u€\n",
                          logBaseRequestInfo(id, buffer, request),
                          WIDTH_ACCOUNT, request->value.transfer.account_id,
                          WIDTH_BALANCE, request->value.transfer.amount);
    case OP_SHUTDOWN: return atomicPrintf(fd, "%s\n", logBaseRequestInfo(id, buffer, request));
    default: break;
  }

  return -2;
}

int logReply(int fd, int id, const tlv_reply_t *reply) {
  if (!reply)
    return -1;

  char buffer[PIPE_BUF];

  switch (reply->type) {
    case OP_CREATE_ACCOUNT: return atomicPrintf(fd, "%s\n", logBaseReplyInfo(id, buffer, reply));
    case OP_BALANCE:
      return atomicPrintf(fd, "%s %*d€\n", logBaseReplyInfo(id, buffer, reply),
                          WIDTH_BALANCE, reply->value.balance.balance);
    case OP_TRANSFER:
      return atomicPrintf(fd, "%s %*d€\n", logBaseReplyInfo(id, buffer, reply),
                          WIDTH_BALANCE, reply->value.transfer.balance);
    case OP_SHUTDOWN:
      return atomicPrintf(fd, "%s %d\n", logBaseReplyInfo(id, buffer, reply),
                          reply->value.shutdown.active_offices);
    default: break;
  }

  return -2;
}

int logBankOfficeOpen(int fd, int id, pthread_t tid) {
  return logBankOfficeInfo(fd, id, tid, true);
}

int logBankOfficeClose(int fd, int id, pthread_t tid) {
  return logBankOfficeInfo(fd, id, tid, false);
}

int logAccountCreation(int fd, int id, const bank_account_t *account) {
  if (!account)
    return -1;

  return atomicPrintf(fd, "I - %0*d - %0*d %*s ...%*s\n",
                      WIDTH_ID, id, WIDTH_ACCOUNT, account->account_id,
                      SALT_LEN, account->salt, WIDTH_HASH, &account->hash[HASH_LEN - WIDTH_HASH]);
}

int logSyncMech(int fd, int id, sync_mech_op_t smo, sync_role_t role, int sid) {
  return atomicPrintf(fd, "S - %0*d - %c%0*d %s\n",
                      WIDTH_ID, id, SYNC_ROLE_STR[role][0], WIDTH_ID, sid, SYNC_MECH_STR[smo]);
}

int logSyncMechSem(int fd, int id, sync_mech_op_t smo, sync_role_t role, int sid, int val) {
  return atomicPrintf(fd, "S - %0*d - %c%0*d %s [val=%d]\n",
                      WIDTH_ID, id, SYNC_ROLE_STR[role][0], WIDTH_ID, sid, SYNC_MECH_STR[smo], val);
}

int logDelay(int fd, int id, uint32_t delay_ms) {
  return atomicPrintf(fd, "A - %0*d - [%*u ms]\n", WIDTH_ID, id, WIDTH_DELAY, delay_ms);
}

int logSyncDelay(int fd, int id, int sid, uint32_t delay_ms) {
  return atomicPrintf(fd, "A - %0*d - %c%0*d [%*u ms]\n",
                      WIDTH_ID, id, SYNC_ROLE_STR[SYNC_ROLE_ACCOUNT][0],
                      WIDTH_ID, sid, WIDTH_DELAY, delay_ms);
}

/*
 * Ancillary functions
 */

static int atomicPrintf(int fd, const char *format, ...) {
  //static char buffer[PIPE_BUF]; // replaced
  char buffer[PIPE_BUF];
  va_list args;
  int ret;

  va_start(args, format);
  ret = vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);

  write(fd, buffer, strlen(buffer));

  return ret;
}

static char *logBaseRequestInfo(int id, char *str, const tlv_request_t *request) {
  const int pass_padding = MAX_PASSWORD_LEN - (int) strlen(request->value.header.password);
  const bool received = (id != getpid());

  snprintf(str, PIPE_BUF, "%c - %0*d - [%*d bytes] %0*d (%0*d, \"%s\")%*s [%*u ms] %-*s",
           (received ? 'R' : 'E'), WIDTH_ID, id, WIDTH_TLV_LEN, request->length,
           WIDTH_ID, request->value.header.pid, WIDTH_ACCOUNT, request->value.header.account_id,
           request->value.header.password, pass_padding, "",
           WIDTH_DELAY, request->value.header.op_delay_ms, WIDTH_OP, OP_TYPE_STR[request->type]);

  return str;
}

static char *logBaseReplyInfo(int id, char *str, const tlv_reply_t *reply) {
  const bool received = (id == getpid());

  snprintf(str, PIPE_BUF, "%c - %0*d - [%*d bytes] %0*d %*s %*s", (received ? 'R' : 'E'),
           WIDTH_ID, id, WIDTH_TLV_LEN, reply->length,
           WIDTH_ACCOUNT, reply->value.header.account_id,
           WIDTH_OP, OP_TYPE_STR[reply->type], WIDTH_RC, RC_STR[reply->value.header.ret_code]);

  return str;
}

static int logBankOfficeInfo(int fd, int id, pthread_t tid, bool open) {
  return atomicPrintf(fd, "I - %0*d - %-*s %lu\n",
                      WIDTH_ID, id, WIDTH_STARTEND, (open ? "OPEN" : "CLOSE"), tid);
}
