#ifndef _chttpd_H
#define _chttpd_H
#include <cjson/cJSON.h>
#include <pthread.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define STDIN   0
#define STDOUT  1
#define STDERR  2

#define METHOD_GET	0//请求指定资源的表示形式。通常用于获取数据，不应产生副作用。
#define METHOD_POST	1//向指定资源提交数据，通常用于创建新资源或触发处理操作。
#define METHOD_PUT	2//替换或更新指定资源的所有内容。
#define METHOD_DELETE	3//删除指定资源。
#define METHOD_HEAD	4//类似于 GET，但只返回响应头，不返回响应体。用于获取资源的元信息。
#define METHOD_OPTIONS	5//返回服务器支持的 HTTP 方法。用于跨域请求预检（CORS）。
#define METHOD_PATCH	6//对资源进行部分更新。
#define METHOD_TRACE	7//回显服务器收到的请求，主要用于测试或诊断。
#define METHOD_CONNECT	8//将连接转换为隧道（通常用于 HTTPS 代理）。
#define METHOD_LINK	9//建立资源之间的关联（已弃用）。
#define METHOD_UNLINK	10//删除资源之间的关联（已弃用）。
#define METHOD_PURGE	11//清除缓存中的资源（通常用于 CDN 或缓存服务器）。
#define METHOD_LOCK	12//锁定资源，防止其他客户端修改（用于 WebDAV）。
#define METHOD_UNLOCK	13//解锁资源（用于 WebDAV）。
#define METHOD_PROPFIND	14//检索资源的属性（用于 WebDAV）。
#define METHOD_PROPPATCH	15//修改资源的属性（用于 WebDAV）。
#define METHOD_MKCOL	16//创建集合（目录）（用于 WebDAV）。
#define METHOD_COPY	17//复制资源（用于 WebDAV）。
#define METHOD_MOVE	18//移动资源（用于 WebDAV）。

#define CONTENTTYPE_HTML	1
#define CONTENTTYPE_TEXT	2
#define CONTENTTYPE_JSON	3

#define ERR_EXIT_NO 0
#define ERR_EXIT_PTHREAD 1
#define ERR_EXIT_PROCESS 2

typedef struct chttpd_s chttpd_t;

typedef void (*chttpd_ptr1func)(void*);
typedef void (*chttpd_ptr2func)(void*,void*);

#ifndef NODE_ARGC
#define NODE_ARGC 24
#endif

struct node_s{
#define NODE_S(T)	T* root;\
	T* next;\
	T* prev;
	NODE_S(struct node_s)
	void* userdata[NODE_ARGC-3];
};
typedef struct node_s node_t;

struct chttpd_s{
	/* 3 8bytes*/
	NODE_S(struct chttpd_s)
	union{
#define CHTTPD_FLAG_U		uint64_t _;\
		/*common*/\
		struct{\
			uint64_t userdatatype0:4;\
			uint64_t userdatatype1:4;\
			uint64_t userdatatype2:4;\
			uint64_t userdatatype3:4;\
			uint64_t userdatatype4:4;\
			uint64_t userdatatype5:4;\
			uint64_t userdatatype6:4;\
			uint64_t userdatatype7:4;\
			uint64_t err_exit:2;\
			uint64_t branch:30;\
		};\
		/*root*/\
		struct{\
			uint64_t userdatatype_all:32;\
			uint64_t :2;\
			uint64_t port:16;\
			uint64_t signal:6;\
			uint64_t :8;\
		};\
		/*res*/\
		struct{\
			uint64_t :34;\
			uint64_t contenttype:2;\
			uint64_t is_file:1;\
			uint64_t codex:2;\
			uint64_t codeyy:5;\
			uint64_t :20;\
		};\
		/*req*/\
		struct{\
			uint64_t :34;\
			uint64_t :2;/*contenttype*/\
			uint64_t err:3;\
			uint64_t method:5;\
			uint64_t :20;\
		};
	/* 1 8bytes*/
		CHTTPD_FLAG_U
	};
#define CHTTPD_S	chttpd_ptr1func process_exit_func;\
	chttpd_ptr1func pthread_exit_func;\
	chttpd_ptr1func delete_func;\
	chttpd_ptr2func run_func;\
	pthread_t tid;\
	int64_t client;\
	uint64_t nbody;\
	char* body;\
	char* url;\
	char* path;\
	cJSON* header;\
	cJSON* argument;
	/* 12 8bytes*/
	CHTTPD_S
	void* userdata[NODE_ARGC-3-1-12];
};
typedef struct chttpd_s chttpd_t;

void node_add_after(chttpd_t*,chttpd_t*);
void node_add_tail(chttpd_t*,chttpd_t*);
void chttpd_delete(chttpd_t* h);
void chttpd_destroy(chttpd_t* h);
void chttpd_error(chttpd_t* h,const char *);
void chttpd_exit_process(chttpd_t* h);
void chttpd_exit_pthread(chttpd_t* h);
void chttpd_print(chttpd_t* h);
void chttpd_request(void *arg);
int chttpd_request_get_line(int client, char *buf, int size);
void chttpd_response(chttpd_t*);
void chttpd_startup(chttpd_t* h);
void chttpd_transferfile(chttpd_t*);

#endif