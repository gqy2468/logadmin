/*
 * Util for logadmin.
 */
#include "util.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
  
#include <fcntl.h>

/** 
 * mime_type - get mime type header 
 * 
 */  
char * mime_type(char *name)
{  
    char *dot, *buf;
  
    dot = strrchr(name, '.');
  
    /* Text */  
    if ( strcmp(dot, ".txt") == 0 ){  
        buf = "text/plain";
    } else if ( strcmp( dot, ".css" ) == 0 ){  
        buf = "text/css";
    } else if ( strcmp( dot, ".js" ) == 0 ){  
        buf = "text/javascript";
    } else if ( strcmp(dot, ".xml") == 0 || strcmp(dot, ".xsl") == 0 ){  
        buf = "text/xml";
    } else if ( strcmp(dot, ".xhtm") == 0 || strcmp(dot, ".xhtml") == 0 || strcmp(dot, ".xht") == 0 ){  
        buf = "application/xhtml+xml";
    } else if ( strcmp(dot, ".html") == 0 || strcmp(dot, ".htm") == 0 || strcmp(dot, ".shtml") == 0 || strcmp(dot, ".hts") == 0 ){  
        buf = "text/html";
  
    /* Images */  
    } else if ( strcmp( dot, ".gif" ) == 0 ){  
        buf = "image/gif";
    } else if ( strcmp( dot, ".png" ) == 0 ){  
        buf = "image/png";
    } else if ( strcmp( dot, ".bmp" ) == 0 ){  
        buf = "application/x-MS-bmp";
    } else if ( strcmp( dot, ".jpg" ) == 0 || strcmp( dot, ".jpeg" ) == 0 || strcmp( dot, ".jpe" ) == 0 || strcmp( dot, ".jpz" ) == 0 ){  
        buf = "image/jpeg";
    } else {  
        buf = "application/octet-stream";
    }
	
	return buf;
}

/** 
 * show_help - show application help 
 * 
 */ 
void show_help() {
	char *help = "Usage: logadmin [options]\n"
		"-l <ip_addr> interface to listen on, default is 127.0.0.1\n"
		"-p <num>     port number to listen on, default is 8706\n"
		"-d           run as a deamon\n"
		"-s           run as a socket server\n"
		"-t <second>  timeout for a http request, default is 60 seconds\n"
		"-h           print this help and exit\n"
		"\n";
	fprintf(stderr, help);
}

/** 
 * show_version - show applicationshow_version 
 * 
 */ 
void show_version() {
	char *version = "MoreLog 1.0.0 (built: May 16 2016 12:15:10)\n"
		"Copyright (c) 2009-2016 The Rschome.com"
		"\n";
	fprintf(stderr, version);
}

/** 
 * exec_cmd - execute shell command 
 * 
 */ 
char * exec_cmd(char *cmd) {
    int len = 1024;
	char *str = malloc(len + 1);//注意多一位给\0使用
	char *buf = malloc(len + 1);
	strcpy(buf, "");
	if(!cmd || !strlen(cmd)) return buf;
    FILE *pp = popen(cmd, "r"); //建立管道
    if (!pp) {
		fprintf(stderr, "execute command error: “%s”\n", cmd);
        return buf;
    }
	while (!feof(pp)) {
		memset(str, 0, len + 1);
		fread(str, sizeof(char), len, pp);
		fseek(pp, strlen(str), SEEK_CUR);
		if (strlen(buf) > 0) {
			buf = (char *)realloc(buf, strlen(buf) + strlen(str) + 1);
			strcat(buf, str);
		} else {
			strcpy(buf, str);
		}
	}
    pclose(pp); //关闭管道
	free(str);
    return buf;
}

/** 
 * read_log - read log file
 * 
 */ 
char * read_log(char *path) {
	if(!path || !strlen(path)) return "";
    int fd = open(path, O_RDONLY);
    if (!fd) {
		fprintf(stderr, "read log error: %s\n", path);
        return "";
    }
	int size = file_size(path);
	char *buf = malloc(size + 1);
    read(fd, buf, size);
	buf[size] = '\0';
    close(fd);
    return buf;
}

/** 
 * joinstr - join string
 * 
 */ 
char * joinstr(char *str, ...) {
	if (str == NULL) return str;
    int key = 0;
    int len = strlen(str) + 1;
	char *p, *sval;
	int ival;
	double dval;
    char *res = malloc(len);
    char *s = malloc(len);
	strcpy(res, "");
    va_list args;
    va_start(args, str);
	for (p = str; *p; p++) {
		if(*p != '%') {
			s[key++] = *p;
			continue;
		}
		switch(*++p) {
			case '%':
				s[key] = '%';
				s[key+1] = '\0';
				strcat(res, s);
				key = 0;
				break;
			case 'd':
				if (key > 0) {
					s[key] = '\0';
					strcat(res, s);
					key = 0;
				}
				ival = va_arg(args, int);
				char *isval = malloc(10);
				sprintf(isval, "%d", ival); 
				len = len - 2 + strlen(isval);
				res = (char *)realloc(res, len);
				strcat(res, isval);
				free(isval);
				break;
			case 'f':
				if (key > 0) {
					s[key] = '\0';
					strcat(res, s);
					key = 0;
				}
				dval = va_arg(args, double);
				char *dsval = malloc(50);
				sprintf(dsval, "%f", dval); 
				len = len - 2 + strlen(dsval);
				res = (char *)realloc(res, len);
				strcat(res, dsval);
				free(dsval);
				break;
			case 's':
				if (key > 0) {
					s[key] = '\0';
					strcat(res, s);
					key = 0;
				}
				sval = va_arg(args, char *);
				len = len - 2 + strlen(sval);
				res = (char *)realloc(res, len);
				strcat(res, sval);
				break;
			default:;
		}
	}
	if (key != 0) {
		s[key] = '\0';
		strcat(res, s);
		key = 0;
	}
	va_end(args);
	free(s);
    return res;  
}