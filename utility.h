#pragma once

#include <sys/times.h>

/** @defgroup Utility Utility
 * @{
 *
 * 
 */

/* Macros */
#define READ 0
#define WRITE 1
#define PATH_LENGTH 258
#define BUFFER_SIZE 512
#define STRING_MAX 1024

/**  */
typedef struct
{
  /** @brief recursive */
  int r;
  /** @brief md5 hash */
  int md5;
  /** @brief sha1 hash */
  int sha1;
  /** @brief sha256 hash */
  int sha256;
  /** @brief Write in an outfile */
  int o;
  /** @brief logfile */
  int v;

} opt_t;

typedef struct
{
  /**@brief name of the logFile*/
  char *logFile;
  /**@brief File descriptor*/
  int fileDescriptor;
  /**@brief time started counting*/
  clock_t start;
  /**@brief time finished counting*/
  clock_t end;
  /**@brief struct time*/
  struct tms time;

} log_t;

/** @brief Formats the value to a date complying with the ISO 8601 format, <date>T<time>
 * 
 *  @param s String (char array) to where the output will be written
 *  @param val Time value as in struct stat st_time fields
 * 
 *  @return Reference to string s
*/
char *formatDate(char s[], time_t val)
{
  strftime(s, 20, "%Y-%m-%dT%X", localtime(&val));
  return s;
}

/** @brief Formats the file permissions value into a string containing the user permissions
 *  
 *  @param s String (char array) to where the output is written
 *  @param st_mode Struct stat st_mode field value (holds the file permissions)
 * 
 *  @return Reference to string s
 */
char *formatPermissions(char s[], int st_mode)
{
  char str[4];
  int i = -1;

  if (st_mode & S_IRUSR)
  { //User has reading permission
    str[++i] = 'r';
  }

  if (st_mode & S_IWUSR)
  { //User has writing permission
    str[++i] = 'w';
  }

  if (st_mode & S_IXUSR)
  { //User has executing permission
    str[++i] = 'x';
  }

  str[++i] = '\0';

  return strcpy(s, str);
}

/** @brief Checks if a directory is either '.' or '..'
 * 
 *  @param directory Directory name
 * 
 *  @return True if it isn't '.' nor '..', False otherwise
 */
int notPoint(const char *directory)
{
  return strcmp(".", directory) && strcmp("..", directory);
}

/**@}*/