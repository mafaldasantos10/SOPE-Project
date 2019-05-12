#pragma once

/*
 * ================================================================================================
 * USER Related Utility
 * ================================================================================================
 */

/**
 * @brief Verifies if the indicated accountID follows the correct nomenclature
 * 
 * @param char *accountID Given AccountID
 */
void validateAccountID(char *accountID);

/**
 * @brief Verifies if the indicated balance is valid
 * 
 * @param char *balance Given balance
 */
void validateBalance(char *balance);

/**
 * @brief Verifies if the indicated password follows the correct nomenclature
 * 
 * @param char *pass Given password
 */
void validatePassword(char *pass);

/**
 * @brief Verifies if the indicated amount is valid
 * 
 * @param char* amount Given Amount
 */
void validateAmount(char *amount);

/**
 * @brief Verifies if the indicated delay is valid
 * 
 * @param char* delay Given Delay
 */
void validateDelay(char *delay);

/*
 * ================================================================================================
 * SERVER Related Utility
 * ================================================================================================
 */

/**
 * @brief Generates a random set of hexadecimal numbers (salt)
 * 
 * @return char *salt Salt
 */
char *generateSalt();

/**
 * @brief Generates a encription made with the salt and password
 * 
 * @param char *salt Used for the encription
 * @param char *password Used for the encription
 * @return char *encripted Generated hash
 */
char *generateHash(char *salt, char *password);