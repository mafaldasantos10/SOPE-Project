#pragma once

/** @defgroup Utility Utility
 * @{
 *
 * 
 */

typedef struct {
  /** @brief recursive */
  int r;
  /** @brief md5 hash */
  int md5;
  /** @brief sha1 hash */
  int sha1;
  /** @brief sha256 hash */
  int sha256;
  /** @brief Write in a outfile */
  int o;
  /** @brief logfile */
  int v;
} Mode;

/**@}*/