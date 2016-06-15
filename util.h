/*
 * Util header for logadmin.
 */
void mime_type(const char *name, char *ret);
void show_help();
void show_version();
int exec_cmd(char *cmd, char *buf, int len);
int read_log(char *log, char *buf, int page, int len);