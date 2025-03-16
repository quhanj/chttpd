#include <cdelete.h>
#include <chttpd.h>
#include <pthread.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

void node_add_after(chttpd_t* h,chttpd_t* prev){
	if(prev->next){
		prev->next->prev=h;
		h->next=prev->next;
	}
	prev->next=h;
	h->prev=prev;
	h->root=prev->root;
}
void node_add_tail(chttpd_t* h,chttpd_t* root){
	chttpd_t* prev=root;
	while(prev->next!=NULL)prev=prev->next;
	node_add_after(h,prev);
}
void chttpd_delete(chttpd_t* h){
	if(h==h->root)chttpd_destroy(h);
	if(h->delete_func)h->delete_func(h);
	cdelete(h->url,CDBUF);
	cdelete(h->body,CDBUF);
	cdelete(h->path,CDBUF);
	cdelete(h->header,CDJSON);
	cdelete(h->argument,CDJSON);
	uint8_t t;
	for(uint8_t i=0;i<8;i++){
		t=(h->userdatatype_all>>(i*4))&0xf;
		if(!t)continue;
		cdelete(h->userdata[i],t);
	}
	if(h->prev)h->prev->next=h->next;
	if(h->next)h->next->prev=h->prev;
}
void chttpd_destroy(chttpd_t* h){
	chttpd_t* root=h->root;
	if(h==root)h=h->next;
	while(h){
		if(h->delete_func)h->delete_func(h);
		chttpd_delete(h);
		h=h->next;
	}
	root->root=NULL;
	chttpd_delete(root);
}
void chttpd_error(chttpd_t* h,const char *sc){
	perror(sc);
	if(h->err_exit==ERR_EXIT_PTHREAD)chttpd_exit_pthread(h);
	if(h->err_exit==ERR_EXIT_PROCESS)chttpd_exit_process(h);
}
void chttpd_exit_process(chttpd_t* h){
	uint8_t signal=h->root->signal+1;
	if(h->process_exit_func)h->process_exit_func(h);
	chttpd_destroy(h);
	exit(signal);
}
void chttpd_exit_pthread(chttpd_t* h){
	if(h->pthread_exit_func)h->pthread_exit_func(h);
	pthread_t tid = pthread_self();
	h=h->root->next;
	chttpd_t* next=h;
	while(h->next){
		next=h->next;
		if(h->tid==tid)chttpd_delete(h);
		h=next;
	}
	pthread_exit(NULL);
}
void chttp_print(chttpd_t* h){
	char *headerstring = cJSON_Print(h->header);
	printf("error: %d\nmethod: %d;\nurl: %s;\ncontenttype: %d\nheaders:\n%s\n",h->err,h->method,h->url,h->contenttype,headerstring);
	if(h->nbody>0&&h->body)
		printf("body: %s\n",h->body);
	free(headerstring);
}
void chttpd_request(void *arg){
	chttpd_t* req=arg;
	req->tid=pthread_self();
	chttpd_t res={0};
	memcpy(&res,arg,sizeof(chttpd_t)-sizeof(res.userdata));
	// memset(&res.userdata,0,sizeof(res.userdata));
	node_add_after(&res,req);
	char buf[1024];
	char val0[512];
	char val1[512];
	int nbuf,nrest,nscan;
	cJSON* obj;
	nbuf=chttpd_request_get_line(req->client,buf,sizeof(buf));
	if(nbuf<=0){
		req->err=1;//获得不到第一行信息
		return;
	}
	if((nscan=sscanf(buf, "%s %s %*s", val0,val1))!=2){
		req->err=2;//第一行信息无效
		return;
	}
	req->method=31;
	if(!strcasecmp(val0,"GET"))req->method=METHOD_GET;
	else if(!strcasecmp(val0,"POST"))req->method=METHOD_POST;
	else if(!strcasecmp(val0,"PUT"))req->method=METHOD_PUT;
	else if(!strcasecmp(val0,"DELETE"))req->method=METHOD_DELETE;
	else if(!strcasecmp(val0,"HEAD"))req->method=METHOD_HEAD;
	else if(!strcasecmp(val0,"OPTIONS"))req->method=METHOD_OPTIONS;
	else if(!strcasecmp(val0,"PATCH"))req->method=METHOD_PATCH;
	else if(!strcasecmp(val0,"TRACE"))req->method=METHOD_TRACE;
	else if(!strcasecmp(val0,"CONNECT"))req->method=METHOD_CONNECT;
	else if(!strcasecmp(val0,"LINK"))req->method=METHOD_LINK;
	else if(!strcasecmp(val0,"UNLINK"))req->method=METHOD_UNLINK;
	else if(!strcasecmp(val0,"PURGE"))req->method=METHOD_PURGE;
	else if(!strcasecmp(val0,"LOCK"))req->method=METHOD_LOCK;
	else if(!strcasecmp(val0,"UNLOCK"))req->method=METHOD_UNLOCK;
	else if(!strcasecmp(val0,"PROPFIND"))req->method=METHOD_PROPFIND;
	else if(!strcasecmp(val0,"PROPPATCH"))req->method=METHOD_PROPPATCH;
	else if(!strcasecmp(val0,"MKCOL"))req->method=METHOD_MKCOL;
	else if(!strcasecmp(val0,"COPY"))req->method=METHOD_COPY;
	else if(!strcasecmp(val0,"MOVE"))req->method=METHOD_MOVE;
	if(req->method==31){
		req->err=3;//未知METHOD
		return;
	}
	nscan=strlen(val1);
	if(!nscan){
		req->err=4;//没有URL
		return;
	}
	nscan++;
	req->url=malloc(nscan);
	memcpy(req->url,val1,nscan);
	while(nbuf>0&&strcmp("\n", buf)){
		// if(strcmp(buf,"\r\n\r\n"))break;
		if(!req->header)req->header=cJSON_CreateObject();
		nbuf=chttpd_request_get_line(req->client,buf,sizeof(buf));
		if((nscan=sscanf(buf, "%[^:\r\n]: %s", val0,val1))==2){
			cJSON_AddItemToObject(req->header,val0,cJSON_CreateString(val1));
		}
	}
	if(req->method!=METHOD_GET&&req->method!=METHOD_POST){
		req->err=5;//METHOD不能响应
		return;
	}
	obj=cJSON_GetObjectItem(req->header,"Content-Type");
	if(obj==NULL)req->contenttype=2;
	else if(!strcasecmp(obj->valuestring,"text/plain"))req->contenttype=1;
	else if(!strcasecmp(obj->valuestring,"text/html"))req->contenttype=2;
	else if(!strcasecmp(obj->valuestring,"application/json"))req->contenttype=3;
	else req->contenttype=0;
	if(req->method==METHOD_POST){
		obj=cJSON_GetObjectItem(req->header,"Content-Length");
		if(obj==NULL){
			req->err=6;//没有长度信息
			return;
		}
		nscan=atoi(obj->valuestring);
		if(!nscan){
			req->err=7;//长度信息不是数字
			return;
		}
		cJSON_ReplaceItemInObject(req->header, "Content-Length",cJSON_CreateNumber(nscan));
		req->nbody=0;
		req->body=malloc(nscan<<1);
		nbuf=1024;
		while(req->nbody<(uint64_t)nscan&&nbuf>0){
			nrest=(nscan<<1)-req->nbody;
			nbuf=recv(req->client,&req->body[req->nbody],nrest>1024?1024:nrest,0);
			if(nbuf>0){
				req->nbody+=nbuf;
			}
		}
		req->body[req->nbody]=0;
	}
	if(req->contenttype==1||req->contenttype==3)res.contenttype=req->contenttype;
	else res.contenttype=2;
	switch(req->err){
	case 0:if(req->run_func!=NULL)(req->run_func)(req,&res);break;
	case 1:case 2:case 4:res.codex=2;res.codeyy=16;break;
	case 3:case 5:res.codex=2;res.codeyy=5;break;
	case 6:case 7:res.codex=2;res.codeyy=11;break;
	}
	// http_data_print(&res);
	chttpd_delete(req);
	chttpd_response(&res);
}
int chttpd_request_get_line(int client, char *buf, int size){
	int i = 0;
	char c = '\0';
	int n;
	while ((i < size - 1) && (c != '\n')){
		n = recv(client, &c, 1, 0);
		/* DEBUG printf("%02X\n", c); */
		if (n > 0){
			if (c == '\r'){
				n = recv(client, &c, 1, MSG_PEEK);
				/* DEBUG printf("%02X\n", c); */
				if ((n > 0) && (c == '\n'))
					recv(client, &c, 1, 0);
				else
					c = '\n';
			}
			buf[i] = c;
			i++;
		}else
			c = '\n';
	}
	buf[i] = '\0';
	return(i);
}
void chttpd_response(chttpd_t* res){
	static const char *codestring2xx[7] = {"OK","Created","Accepted","Non-Authoritative Information","No Content","Reset Content","Partial Content"};
	static const char *codestring3xx[8] = {"Multiple Choices","Moved Permanently","Found","See Other","Not Modified","Use Proxy","Unused","Temporary Redirect"};
	static const char *codestring4xx[19] = {"Bad Request","Unauthorized","Payment Required","Forbidden","Not Found","Method Not Allowed","Not Acceptable","Proxy Authentication Required","Request Time-out","Conflict","Gone","Length Required","Precondition Failed","Request Entity Too Large","Request-URI Too Large","Unsupported Media Type","Requested range not satisfiable","Expectation Failed","I'm a teapot"};
	static const char *codestring5xx[6] = {"Internal Server Error","Not Implemented","Bad Gateway","Service Unavailable","Gateway Time-out","HTTP Version not supported"};
	static const char *const*const codestring[4]={codestring2xx,codestring3xx,codestring4xx,codestring5xx};
	cJSON* obj;
	char header[1024];
	char* contenttype;
	if(res->codex==0&&res->codeyy==0){
		if((res->body==NULL||res->nbody<=0)&&!res->is_file){
			res->codex=0;res->codeyy=4;
		}
	}
	uint16_t code=(res->codex+2)*100+res->codeyy;
	const char *codes=codestring[res->codex][res->codeyy];
	if(code==200){
		switch(res->contenttype){
		case 0:
			obj=cJSON_GetObjectItem(res->header,"Content-Type");
			if(obj==NULL)contenttype="text/html";
			else contenttype=obj->valuestring;
			break;
		case 1:contenttype="text/plain";break;
		case 2:contenttype="text/html";break;
		case 3:contenttype="application/json";break;
		}
	}else{
		res->body=malloc(1024);
		switch(res->contenttype){
		case 1:
			contenttype="text/plain";
			sprintf(res->body,"%u %s\r\n",code, codes);
			break;
		case 3:
			contenttype="application/json";
			sprintf(res->body, "{\"error\": %u}",code);
			break;
		default:
			contenttype="text/html";
			sprintf(res->body, "<html><head><title>%s</title></head><body><h1>%u %s</h1></body></html>\r\n",codes,code,codes);
			break;
		}
		res->nbody=strlen(res->body);
	}
	time_t now=time(NULL);
	struct tm *timeinfo=localtime(&now);
	char timebuf[20];
	strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S", timeinfo);
	#ifdef __linux__
	sprintf(header, "HTTP/1.1 %d %s\r\nServer: mychttpd\r\nDate: %s\r\nContent-Type: %s\r\nContent-Length: %lu\r\n\r\n",code,codes,timebuf,contenttype,res->nbody);
	#elif __APPLE__
	sprintf(header, "HTTP/1.1 %d %s\r\nServer: mychttpd\r\nDate: %s\r\nContent-Type: %s\r\nContent-Length: %llu\r\n\r\n",code,codes,timebuf,contenttype,res->nbody);
	#endif
	send(res->client, header, strlen(header), 0);
	if(res->is_file)chttpd_transferfile(res);
	else send(res->client, res->body, res->nbody, 0);
	close(res->client);
	chttpd_delete(res);
}
void chttpd_startup(chttpd_t* root){
	root->err_exit=ERR_EXIT_PROCESS;
	int server_sock = -1;
	// int client_sock = -1;
	struct sockaddr_in client_name;
	socklen_t  client_name_len = sizeof(client_name);
	pthread_t newthread;
	int on = 1;
	struct sockaddr_in name;

	server_sock = socket(PF_INET, SOCK_STREAM, 0);
	if (server_sock == -1)chttpd_error(root,"socket");
	memset(&name, 0, sizeof(name));
	name.sin_family = AF_INET;
	name.sin_port = htons(root->port);
	name.sin_addr.s_addr = htonl(INADDR_ANY);
	if ((setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on))) < 0)chttpd_error(root,"setsockopt failed");
	if (bind(server_sock, (struct sockaddr *)&name, sizeof(name)) < 0)chttpd_error(root,"bind");
	if (root->port == 0){  /* if dynamically allocating a port */
		socklen_t namelen = sizeof(name);
		if (getsockname(server_sock, (struct sockaddr *)&name, &namelen) == -1)chttpd_error(root,"getsockname");
		root->port = ntohs(name.sin_port);
	}
	if (listen(server_sock, 5) < 0)chttpd_error(root,"listen");
	printf("httpd running on port %d\n",root->port);
	while (1){
		int client = accept(server_sock,(struct sockaddr *)&client_name,&client_name_len);
		if (client == -1)chttpd_error(root,"accept");
		chttpd_t req={0};
		memcpy(&req,root,sizeof(chttpd_t)-sizeof(req.userdata));
		// memset(&req.userdata,0,sizeof(req.userdata));
		req.err_exit=ERR_EXIT_PTHREAD;
		req.client=client;
		req.branch=0;
		node_add_tail(&req,root);
		/* accept_request(&client_sock); */
		if(pthread_create(&newthread,NULL,(void*)chttpd_request,(void*)&req)!=0)chttpd_error(root,"pthread_create");
		pthread_join(newthread, NULL);
	}
	close(server_sock);
}
void chttpd_transferfile(chttpd_t* h){
	char buf[1024];
	size_t nread;
	FILE* file=fopen(h->path,"rb");
	if(!file){
		perror(h->path);
		return;
	}
	while ((nread = fread(buf, 1, sizeof(buf), file)) > 0) {
		// 必须循环发送，直到所有数据写入 socket
		size_t total_sent = 0;
		while (total_sent < nread) {
			ssize_t sent = send(h->client, buf + total_sent, nread - total_sent, 0);
			if (sent == -1) {
				perror("send failed");
				return;
			}
			total_sent += sent;
		}
	}
	// 检查是否因错误退出循环
	if (ferror(file))chttpd_error(h,"fread error");
	cdelete(file,CDFOP);
}