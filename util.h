/*
 * Util header for logadmin.
 */
char * mime_type(char *name);
void show_help();
void show_version();
char * exec_cmd(char *cmd);
char * read_log(char *path);
char * joinstr(char *str, ...);