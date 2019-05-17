#pragma once

/**
 * @brief Initializes the user program.
 * 
 * @param argc Number of arguments passed throught the command line.
 * @param argv[] Arguments passed throught the command line.
 * @param tlv TLV struct with the request for the server.
 * @param createAccount Struct used for the request to the server.
 * @param transfer Struct used for the request to the server.
 * @param header Struct used for the request to the server.
 * @param value Struct used for the request to the server.
 */
void init(int argc, char *argv[], tlv_request_t *tlv, req_create_account_t *createAccount, req_transfer_t *transfer, req_header_t *header, req_value_t *value);

/**
 * @brief Fills the necessary structs according to the respective operation.
 * 
 * @param op Operation code.
 * @param argList Argument List used for the user operation.
 * @param createAccount Struct used for the request to the server.
 * @param transfer Struct used for the request to the server.
 * @param value Struct used for the request to the server.
 */
void fillOperationInfo(int op, char *argList, req_create_account_t *createAccount, req_transfer_t *transfer, req_value_t *value);

/**
 * @brief Fills the request header.
 * 
 * @param rgv[] Arguments passed throught the command line.
 * @param header Struct used for the request to the server.
 * @param value Struct used for the request to the server.
 */
void fillHeader(char *argv[], req_header_t *header, req_value_t *value);

/**
 * @brief Generates the user FIFO path based on the process PID.
 * 
 * @param tlv Struct with the request information.
 * @return FIFO path.
 */
char *getFIFOname();

/**
 * @brief Writes the request in the server's FIFO.
 * 
 * @param tlv Struct with the request for the server.
 * @param fd Logfile file descriptor.
 * 
 * @return 1 if write was sucessfull, 0 if the server's FIFO could not be opened.
 */
int writeRequest(tlv_request_t *tlv, int fd);

/**
 * @brief Handler for SIG_ALRM signal. Sets the timeout flag to true.
 */
void alarm_handler();

/**
 * @brief Reads the reply from the server.
 * 
 * @param fifo User's FIFO path.
 * @param fd Logfile file descriptor.
 * @param id User's id to be used if there is a timeout.
 */
void readReply(char *fifo, int fd, int id);

/**
 * @brief Prints a human friendly message on screen according to the reply from server.
 */
void printReply(tlv_reply_t reply);