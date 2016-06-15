/*
 *  logadmin - linux log web viewer
 *
 *       http://www.rschome.com/
 *
 *  Copyright 2016 Rschome.com, Inc.  All rights reserved.
 *
 *  Use and distribution licensed under the BSD license.  See
 *  the LICENSE file for full text.
 *
 *  Authors:
 *      Flash Guo <admin@rschome.com>
 */
#include "logadmin.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <error.h>
#include <errno.h>

#include <sys/queue.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>  
#include <unistd.h>  
#include <fcntl.h>
#include <time.h>
#include <signal.h>

#include <glib.h>

#include <event.h>
#include <evhttp.h>

static char dir_root[20000];

struct http_req  
{
    char method[20];
    char request_uri[500];
    char http_version[100];
    char client_ip[20];
    char request_time[2000];
} http_req_line;

/** 
 *  file_size - get file size 
 */
long file_size(const char *filename)
{
    struct stat buf;
    if (!stat(filename, &buf))
    {
        return buf.st_size;
    }  
    return 0;
}

/** 
 *  signal_handler - signal handler
 */
void signal_handler(int sig)
{
	switch (sig) {
		case SIGTERM:
		case SIGHUP:
		case SIGQUIT:
		case SIGINT:
			//终止侦听event_dispatch()的事件侦听循环，执行之后的代码
			event_loopbreak();
			break;
	}
}

/** 
 *  http_handler - http handler
 */ 
void http_handler(struct evhttp_request *req, void *arg)
{
    char buff[20000];
    char real_path[20000];
    char tmp[200000];
    char content_type[2000];
    int fd;
  
    time_t timep;
    struct tm *m;
    struct stat info;
      
    DIR *dir;
    struct dirent *ptr;
  
    struct evbuffer *buf;
    buf = evbuffer_new();
      
    // 分析URL参数
    char *decode_uri = strdup((char*) evhttp_request_uri(req));
	decode_uri = evhttp_decode_uri(decode_uri);
	if (strcmp(decode_uri, "/") == 0) decode_uri = "/index.html";
    sprintf(http_req_line.request_uri, "%s", decode_uri);

    // 返回给浏览器的头信息
    evhttp_add_header(req->output_headers, "Server", "logadmin");
    evhttp_add_header(req->output_headers, "Connection", "close");
  
    // 取得请求时间
    time(&timep);
    m = localtime(&timep);
    sprintf(http_req_line.request_time, "%4d-%02d-%02d %02d:%02d:%02d", (1900+m->tm_year), (1+m->tm_mon), m->tm_mday, m->tm_hour, m->tm_min, m->tm_sec);
  
    // 获取ACTION的值
	struct evkeyvalq params;
	evhttp_parse_query(decode_uri, &params);
    char *action = (char*)evhttp_find_header(&params, "action");
  
    // 处理不同的ACTION
	if (action) {
		evhttp_add_header(req->output_headers, "Content-Type", "text/html; charset=UTF-8");
		if (strcmp(action, "loginfo") == 0) {
			char *logname = (char*)evhttp_find_header(&params, "logname");
			char *pagenum = (char*)evhttp_find_header(&params, "pagenum");
			int pnum = pagenum ? atoi(pagenum) : 1;
			memset(&tmp, 0, sizeof(tmp));
			if (logname) {
				char cmd[2000];
				int psize = 20;
				char *logreg = (char*)evhttp_find_header(&params, "logreg");
				if(!logreg || !strlen(logreg)){
					logreg = ".*";
				}
				sprintf(cmd, "grep '%s' %s -c && grep '%s' %s | tail -n +%d | head -n %d", logreg, logname, logreg, logname, (pnum - 1) * psize, psize);
				exec_cmd(cmd, &tmp, 4096);
			}
			evbuffer_add_printf(buf, "%s", tmp);
			evhttp_send_reply(req, HTTP_OK, "OK", buf);
		} else if(strcmp(action, "loglist") == 0) {
			char *dirname = (char*)evhttp_find_header(&params, "dirname");
			memset(&tmp, 0, sizeof(tmp));
			if (dirname) {
				char cmd[2000];
				sprintf(cmd, "ls -lF %s | awk '{print $9}'", dirname);
				exec_cmd(cmd, &tmp, 4096);
			}
			evbuffer_add_printf(buf, "%s", tmp);
			evhttp_send_reply(req, HTTP_OK, "OK", buf);
		} else if(strcmp(action, "logconf") == 0) {
			memset(&tmp, 0, sizeof(tmp));
			GKeyFile* config = g_key_file_new();
			GKeyFileFlags flags = G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS;
			if (g_key_file_load_from_file(config, "./conf/logadmin.conf", flags, NULL)) {
				int i;
				gchar* host;
				gchar* path;
				gsize length;
				char conf[20000];
				gchar** groups = g_key_file_get_groups(config, &length);
				for (i = 0; i < (int)length; i++) {
					host = g_key_file_get_string(config, groups[i], "host", NULL);
					path = g_key_file_get_string(config, groups[i], "path", NULL);
					if (host && path) {
						strcpy(conf, tmp);
						sprintf(tmp, "%s%s;%s\n", conf, host, path);
					}
				}
			}
			g_key_file_free(config);
			evbuffer_add_printf(buf, "%s", tmp);
			evhttp_send_reply(req, HTTP_OK, "OK", buf);
		} else {
			evbuffer_add_printf(buf, "<html><head><title>870 Error Action</title></head><body><h1 align=\"center\">870 Error Action</h1></body></html>");
			evhttp_send_reply(req, 870, "Error Action", buf);
		}
		evbuffer_free(buf);
		return;
	}
  
    // 获取请求的服务器文件路径
    memset(&real_path, 0, sizeof(real_path));
	getcwd(dir_root, sizeof(dir_root));
    sprintf(real_path, "%s/%s%s", dir_root, DOCUMENT_ROOT, http_req_line.request_uri);

    if(stat(real_path,&info) == -1)
    {
        evhttp_add_header(req->output_headers, "Content-Type", "text/html; charset=UTF-8");
        if (errno == ENOENT)
        {
			evbuffer_add_printf(buf, "<html><head><title>404 Not Found</title></head><body><h1 align=\"center\">404 Not Found</h1></body></html>");
			evhttp_send_reply(req, 404, "Not Found", buf);
        }  
        else if(access(real_path,R_OK) < 0)
        {
			evbuffer_add_printf(buf, "<html><head><title>403 Not Found</title></head><body><h1 align=\"center\">403 Forbidden</h1></body></html>");
			evhttp_send_reply(req, 403, "Forbidden", buf);
        }  
        else  
        {
			evbuffer_add_printf(buf, "<html><head><title>500 Server Error</title></head><body><h1 align=\"center\">500 Server Error</h1></body></html>");
			evhttp_send_reply(req, 500, "Server Error", buf);
        }
    }  
    else if(S_ISREG(info.st_mode))
    {
        memset(&tmp, 0, sizeof(tmp));
        fd = open(real_path, O_RDONLY);
        read(fd, tmp, file_size(real_path));
        close(fd);
  
        // 设置HEADER头并输出内容到浏览器
        memset(&buff, 0, sizeof(buff));
        mime_type(real_path, content_type);
        sprintf(buff, "%s; charset=UTF-8", content_type);
        evhttp_add_header(req->output_headers, "Content-Type", buff);
        evbuffer_add_printf(buf, "%s", tmp);
        evhttp_send_reply(req, HTTP_OK, "OK", buf);
    }

    // 内存释放
    evbuffer_free(buf);
}  
  
int main(int argc, char **argv)
{
	//自定义信号处理函数
	signal(SIGHUP, signal_handler);
	signal(SIGTERM, signal_handler);
	signal(SIGINT, signal_handler);
	signal(SIGQUIT, signal_handler);

	//默认参数
	char *httpd_option_listen = "127.0.0.1";
	int httpd_option_port = 8706;
	int httpd_option_daemon = 0;
	int httpd_option_timeout = 60;

	//获取参数
	int c;
	while ((c = getopt(argc, argv, "l:p:dt:vh")) != -1) {
		switch (c) {
			case 'l' :
				httpd_option_listen = optarg;
				break;
			case 'p' :
				httpd_option_port = atoi(optarg);
				break;
			case 'd' :
				httpd_option_daemon = 1;
				break;
			case 't' :
				httpd_option_timeout = atoi(optarg);
				break;
			case 'v' :
				show_version();
				exit(EXIT_SUCCESS);
			case 'h' :
			default :
				show_help();
				exit(EXIT_SUCCESS);
		}
	}

	//判断是否设置了-d，以daemon运行
	if (httpd_option_daemon) {
		pid_t pid;
		pid = fork();
		if (pid < 0) {
			perror("fork failed");
			exit(EXIT_FAILURE);
		}
		if (pid > 0) {
			//生成子进程成功，退出父进程
			exit(EXIT_SUCCESS);
		}
	}

	//初始化event API
    event_init();

    // 绑定IP和端口
    struct evhttp *httpd;
	httpd = evhttp_start(httpd_option_listen, httpd_option_port);

    if (httpd == NULL)
    {
        fprintf(stderr, "Error: Unable to listen on %s:%d\n", httpd_option_listen, httpd_option_port);
        exit(1);
    }

    // 设置请求超时时间
    evhttp_set_timeout(httpd, httpd_option_timeout);

    // 设置请求的处理函数
    evhttp_set_gencb(httpd, http_handler, NULL);
    event_dispatch();
    evhttp_free(httpd);

    return 0;
}