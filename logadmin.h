/**
 * The main logadmin header holding commonly used data
 * structures and function prototypes.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#define DOCUMENT_ROOT "html"

long file_size(const char *filename);
void signal_handler(int sig);