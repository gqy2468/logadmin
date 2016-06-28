/**
 * The main logadmin header holding commonly used data
 * structures and function prototypes.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#define MEM_SIZE 4096
#define DOCUMENT_ROOT "html"

int set_nonblock(int fd);
long file_size(const char *filename);
void sock_cmd(char *host, int port, char *cmd, char *buf, int len);
void signal_handler(int sig);