#pragma once

void init(int argc, char *argv[], tlv_request_t* tlv);
void fillOperationInfo(int op, char* argList);
void fillHeader(char *argv[]);
void writeRequest(tlv_request_t *tlv);
void readReply(tlv_reply_t reply, char *fifo);
char* getFIFOname();