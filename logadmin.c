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
#include "global.h"
#include "util.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <signal.h>

#ifdef __WIN32
	#include <WinSock2.h>
#else
	#include <sys/queue.h>
	#include <sys/types.h>
	#include <sys/socket.h>
	#include <unistd.h>
	#include <arpa/inet.h>
	#include <netinet/in.h>
	#include <netinet/tcp.h>
	#include <netdb.h>

	#include <glib.h>
#endif

#include <event.h>
#include <evhttp.h>

struct http_req  
{
    char method[20];
    char request_uri[500];
    char http_version[100];
    char client_ip[25];
    char request_time[20];
} http_req_line;

struct sock_ev {
    struct event* read_ev;
    struct event* write_ev;
    char* buffer;
};

/** 
 *  set_nonblock - set nonblock 
 */
int set_nonblock(int fd)
{
#ifdef __WIN32
	unsigned long on_windows = 1;
	return ioctlsocket(fd, FIONBIO, &on_windows);
#else
	char *on = 1;
	return ioctlsocket(fd, FIONBIO, (char *)&on);
#endif
}

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
 *  sock_cmd - execute shell command by socket
 */ 
void sock_cmd(char *host, int port, char *cmd, char *buf, int len)
{
	if (!host || !strlen(host)) {
        return;
    }
    if (port <= 0) {
        port = 8706;
	}
	if (!cmd || !strlen(cmd)) {
        return;
    }
    if (len <= 0) {
        len = 2048;
	}

    // create socket
#ifdef __WIN32
	unsigned short ver;
	WSADATA wsaData;
	ver = MAKEWORD(1, 1);
	WSAStartup(ver, &wsaData);
#endif
	int fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0) {
		fprintf(stderr, "Failed to create socket\n");
		return;
	}

    // connect socket
    struct sockaddr_in my_address;
    memset(&my_address, 0, sizeof(my_address));
    my_address.sin_family = AF_INET;
    my_address.sin_addr.s_addr = inet_addr(host);
    my_address.sin_port = htons(port);
    if (connect(fd, (struct sockaddr*)&my_address, sizeof(my_address)) == -1) {  
		fprintf(stderr, "Failed to connect socket %s:%d\n", host, port);
		return;
    }

    // set socket timeout
	int timeout = 2000; // 2 second timeout
	setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, (char*)&timeout, sizeof(int));
	setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(int));

    // send socket
	if (send(fd, cmd, strlen(cmd), 0) < 0) {  
		fprintf(stderr, "Failed to write socket\n");
		return;
	}

    // recv socket
	memset(buf, 0, len);
	while (1) {
		if (recv(fd, buf, len, 0) < 0) {
#ifdef __WIN32
			if (errno == EAGAIN || errno == EINTR) continue;//继续接收数据
#else
			if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) continue;//继续接收数据
#endif
			fprintf(stderr, "Failed to read socket\n");
			break;//跳出接收循环
		} else {
			break;//跳出接收循环
		}
	}

    close(fd);
#ifdef __WIN32
	WSACleanup();
#endif
}

/** 
 *  signal_handler - signal handler
 */
void signal_handler(int sig)
{
	switch (sig) {
#ifndef __WIN32
		case SIGHUP:
		case SIGQUIT:
#endif
		case SIGTERM:
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
    time_t timep;
    struct tm *m;
    struct stat info;
  
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
		char *tmp = "";
		evhttp_add_header(req->output_headers, "Content-Type", "text/html; charset=UTF-8");
		if (strcmp(action, "loginfo") == 0) {
			char *loghost = (char*)evhttp_find_header(&params, "loghost");
			char *logport = (char*)evhttp_find_header(&params, "logport");
			char *logname = (char*)evhttp_find_header(&params, "logname");
			char *pagenum = (char*)evhttp_find_header(&params, "pagenum");
			int pnum = pagenum ? atoi(pagenum) : 1;
			int port = logport ? atoi(logport) : 8706;
			if (logname) {
				int psize = 20;
				char *logreg = (char*)evhttp_find_header(&params, "logreg");
				if(!logreg || !strlen(logreg)){
					logreg = ".*";
				}
				char *cmd = joinstr("grep '%s' %s -c && grep '%s' %s | tail -n +%d | head -n %d", logreg, logname, logreg, logname, (pnum - 1) * psize, psize);
				if (!loghost || !strlen(loghost)) {
					tmp = exec_cmd(cmd);
				} else {
					sock_cmd(loghost, port, cmd, (char*)&tmp, MEM_SIZE);
				}
			}
			evbuffer_add_printf(buf, "%s", tmp);
			evhttp_send_reply(req, HTTP_OK, "OK", buf);
		} else if(strcmp(action, "loglist") == 0) {
			char *loghost = (char*)evhttp_find_header(&params, "loghost");
			char *logport = (char*)evhttp_find_header(&params, "logport");
			char *dirname = (char*)evhttp_find_header(&params, "dirname");
			int port = logport ? atoi(logport) : 8706;
			if (dirname) {
				char *cmd = joinstr("ls -lF %s | awk '{print $9}'", dirname);
				if (!loghost || !strlen(loghost)) {
					tmp = exec_cmd(cmd);
				} else {
					sock_cmd(loghost, port, cmd, (char*)&tmp, MEM_SIZE);
				}
			}
			evbuffer_add_printf(buf, "%s", tmp);
			evhttp_send_reply(req, HTTP_OK, "OK", buf);
		} else if(strcmp(action, "logconf") == 0) {
#ifndef __WIN32
			GKeyFile* config = g_key_file_new();
			GKeyFileFlags flags = G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS;
			if (g_key_file_load_from_file(config, "./conf/logadmin.conf", flags, NULL)) {
				int i;
				gsize length;
				gchar** groups = g_key_file_get_groups(config, &length);
				for (i = 0; i < (int)length; i++) {
					gchar* host = g_key_file_get_string(config, groups[i], "host", NULL);
					gchar* path = g_key_file_get_string(config, groups[i], "path", NULL);
					gchar* port = g_key_file_get_string(config, groups[i], "port", NULL);
					if(!port || !strlen(port)) port = "8706";
					if (host && path) tmp = joinstr("%s%s:%s;%s\n", tmp, host, port, path);
				}
			}
			g_key_file_free(config);
#endif
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
    char *dir_root = getcwd(NULL, 0);
    char *real_path = joinstr("%s/%s%s", dir_root, DOCUMENT_ROOT, http_req_line.request_uri);

    if (stat(real_path,&info) == -1) {
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
    } else if(S_ISREG(info.st_mode)) {
        // 设置HEADER头并输出内容到浏览器
        evhttp_add_header(req->output_headers, "Content-Type", joinstr("%s; charset=UTF-8", mime_type(real_path)));
        evbuffer_add_printf(buf, "%s", read_log(real_path));
        evhttp_send_reply(req, HTTP_OK, "OK", buf);
    }

    // 内存释放
    evbuffer_free(buf);
    free(dir_root);
}

/** 
 *  free_sock - free socket
 */ 

void free_sock(struct sock_ev* ev)
{
    event_del(ev->read_ev);
    free(ev->read_ev);
    free(ev->write_ev);
    free(ev->buffer);
    free(ev);
}

/** 
 *  on_write - event write
 */ 
void on_write(int sock, short event, void* arg)
{
    char* buffer = (char*)arg;
    send(sock, buffer, strlen(buffer), 0);
    free(buffer);
}

/** 
 *  on_read - event read
 */ 
void on_read(int sock, short event, void* arg)
{
    struct sock_ev* ev = (struct sock_ev*)arg;
    ev->buffer = (char*)malloc(MEM_SIZE);
    memset(ev->buffer, 0, sizeof(char));
    if (recv(sock, ev->buffer, MEM_SIZE, 0) <= 0) {
        free_sock(ev);
        close(sock);
        return;
    }
	char *cmd = joinstr("%s", ev->buffer);
    event_set(ev->write_ev, sock, EV_WRITE, on_write, exec_cmd(cmd));
    event_add(ev->write_ev, NULL);
}

/** 
 *  on_accept - event accept
 */ 
void on_accept(int sock, short event, void* arg)
{
    struct sockaddr_in cli_addr;
    int newfd, sin_size;
    struct sock_ev* ev = (struct sock_ev*)malloc(sizeof(struct sock_ev));
    ev->read_ev = (struct event*)malloc(sizeof(struct event));
    ev->write_ev = (struct event*)malloc(sizeof(struct event));
    sin_size = sizeof(struct sockaddr_in);
    newfd = accept(sock, (struct sockaddr*)&cli_addr, &sin_size);
    event_set(ev->read_ev, newfd, EV_READ|EV_PERSIST, on_read, ev);
    event_add(ev->read_ev, NULL);
}
  
int main(int argc, char **argv)
{
	//自定义信号处理函数
#ifndef __WIN32
	signal(SIGHUP, signal_handler);
	signal(SIGQUIT, signal_handler);
#endif
	signal(SIGTERM, signal_handler);
	signal(SIGINT, signal_handler);

	//默认参数
	char *event_option_listen = "127.0.0.1";
	int event_option_port = 8706;
	int event_option_daemon = 0;
	int event_option_timeout = 60;
	int event_option_socket = 0;

	//获取参数
	int c;
	while ((c = getopt(argc, argv, "l:p:dst:vh")) != -1) {
		switch (c) {
			case 'l' :
				event_option_listen = optarg;
				break;
			case 'p' :
				event_option_port = atoi(optarg);
				break;
			case 'd' :
				event_option_daemon = 1;
				break;
			case 's' :
				event_option_socket = 1;
				break;
			case 't' :
				event_option_timeout = atoi(optarg);
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
	if (event_option_daemon) {
		pid_t pid = 0;
#ifndef __WIN32
		pid = fork();
#endif
		if (pid < 0) {
			perror("Failed to fork\n");
			exit(EXIT_FAILURE);
		}
		if (pid > 0) {
			//生成子进程成功，退出父进程
			exit(EXIT_SUCCESS);
		}
	}

	//初始化event API
    event_init();

	//判断是否设置了-s，以socket运行
	if (event_option_socket) {
		int socketlisten;
		struct sockaddr_in addresslisten;
		struct event accept_event;

#ifdef __WIN32
		unsigned short ver;
		WSADATA wsaData;
		ver = MAKEWORD(1, 1);
		WSAStartup(ver, &wsaData);
#endif
		socketlisten = socket(AF_INET, SOCK_STREAM, 0);
		if (socketlisten < 0) {
			fprintf(stderr, "Failed to create listen socket\n");
			exit(1);
		}

		memset(&addresslisten, 0, sizeof(addresslisten));
		addresslisten.sin_family = AF_INET;
		addresslisten.sin_addr.s_addr = inet_addr(event_option_listen);
		addresslisten.sin_port = htons(event_option_port);
		if (bind(socketlisten, (struct sockaddr *)&addresslisten, sizeof(addresslisten)) < 0) {
			fprintf(stderr, "Failed to bind socket %s:%d\n", event_option_listen, event_option_port);
			exit(1);
		}

		if (listen(socketlisten, 5) < 0) {
			fprintf(stderr, "Failed to listen to socket\n");
			exit(1);
		}
#ifdef __WIN32
		char reuse = 1;
#else
		int reuse = 1;
#endif
		setsockopt(socketlisten, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
		set_nonblock(socketlisten);

		event_set(&accept_event, socketlisten, EV_READ|EV_PERSIST, on_accept, NULL);
		event_add(&accept_event, NULL);
		event_dispatch();

		close(socketlisten);
#ifdef __WIN32
		WSACleanup();
#endif
		return 0;
	}

    // 绑定IP和端口
#ifdef __WIN32
	unsigned short ver;
	WSADATA wsaData;
	ver = MAKEWORD(1, 1);
	WSAStartup(ver, &wsaData);
#endif
    struct evhttp *httpd;
	httpd = evhttp_start(event_option_listen, event_option_port);

    if (httpd == NULL) {
        fprintf(stderr, "Failed to listen on %s:%d\n", event_option_listen, event_option_port);
        exit(1);
    }

    // 设置请求超时时间
    evhttp_set_timeout(httpd, event_option_timeout);

    // 设置请求的处理函数
    evhttp_set_gencb(httpd, http_handler, NULL);
    event_dispatch();
    evhttp_free(httpd);
#ifdef __WIN32
	WSACleanup();
#endif

    return 0;
}
