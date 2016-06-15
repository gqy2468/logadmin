/*
 * Util for logadmin.
 */
#include "logadmin.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/** 
 * mime_type - get mime type header 
 * 
 */  
void mime_type(char *name, char *ret)
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
    strcpy(ret, buf);
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
int exec_cmd(char *cmd, char *buf, int len) {
	if(!cmd || !strlen(cmd)){
        return -1;
    }
    if (len <= 0) {
        len = 2048;
	}
	memset(buf, 0, len);
    FILE *pp = popen(cmd, "r"); //建立管道
    if (!pp) {
        return -1;
    } 
	fread(buf, sizeof(char), len, pp);
    pclose(pp); //关闭管道
    return 1;
}

/** 
 * read_log - read log file by line
 * 
 */ 
int read_log(char *log, char *buf, int page, int len) {
	if(!log || !strlen(log)) {
        return -1;
    }
    if (page <= 0) {
        page = 1;
	}
    if (len <= 0) {
        len = 2048;
	}
	memset(buf, 0, sizeof(buf)); 
    FILE *fp = fopen(log, "r");
	if(fp == NULL) {
        return -1;
	}
	char lines[2048];
	int line = 0;
	while(!feof(fp)) {
		line++;
		memset(lines, 0, sizeof(lines)); 
		if(!fgets(lines, len, fp)) break;
		if(line <= (page - 1) * 20) continue;
		if(line > page * 20) continue;
		strcat(buf, lines);
	}
	char *newbuf = strdup(buf);
	sprintf(buf, "%d\n%s", line, newbuf);
	free(newbuf);
    fclose(fp);
    return 1;
}