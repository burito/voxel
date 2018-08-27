/*
Copyright (c) 2014 Daniel Burke

This software is provided 'as-is', without any express or implied
warranty. In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

   1. The origin of this software must not be misrepresented; you must not
   claim that you wrote the original software. If you use this software
   in a product, an acknowledgment in the product documentation would be
   appreciated but is not required.

   2. Altered source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.

   3. This notice may not be removed or altered from any source
   distribution.
*/

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#ifndef _MSC_VER
typedef int socklen_t;
#endif
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#define SHUT_RDWR SD_BOTH

#else
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netdb.h>
#include <arpa/inet.h>
#endif

#include "log.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#include "3dmaths.h"
#include "gui.h"

#define HTTP_PORT 20000

char http_path[] = "./data/http/";

unsigned int s = 0;


#define MAX_LEN 80
typedef struct http_auth {
	char name[MAX_LEN];
	struct sockaddr_in addr;
	int websocket;
} http_auth;


typedef struct http_pending {
	int request;
	int favicon;
	char get[MAX_LEN];
	char name[MAX_LEN];
	struct sockaddr_in addr;
	int authorised;
} http_pending;


#define HTTP_MAX_CLIENTS 10

http_auth http_authorised_clients[HTTP_MAX_CLIENTS];
int http_num_auth=0;

http_pending http_pending_requests[HTTP_MAX_CLIENTS];
int http_num_pending=0;

int http_accepting_new_clients=0;



int http_init(void)
{
	int ret;
#ifdef _WIN32
	WSADATA wsaData;
	if(WSAStartup(MAKEWORD(2,2),&wsaData))
	{
		return 1;
	}
	const char yes = 1;
#else
	int yes = 1;
#endif
	s = socket(PF_INET, SOCK_STREAM, 0);
	if(-1 == s)
	{
		log_fatal("socket() failed %d", s);
		return 2;
	}


	ret = setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
	if(-1 == ret)
	{
		log_fatal("setsockopt()");
		return 3;
	}

	struct sockaddr_in sin;
//	memset(&sin, 0, sizeof(sin));

	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = INADDR_ANY;
	sin.sin_port = htons(HTTP_PORT);
	ret = bind(s, (struct sockaddr*)&sin, sizeof(struct sockaddr));

	if(-1 == ret)
	{
		log_fatal("bind() = %d", HTTP_PORT);
	}

	ret = listen(s, 5);
	if(-1 == ret)
	{
		log_fatal("listen() = %d", ret);
	}

	log_info("HTTP Host  : OK");
	memset(http_authorised_clients, 0, sizeof(http_auth)*HTTP_MAX_CLIENTS);
	http_num_auth = 0;
	memset(http_pending_requests, 0, sizeof(http_pending)*HTTP_MAX_CLIENTS);
	http_num_pending = 0;
	return 0;
}

void http_end(void)
{
	if(!s)return;
	shutdown(s, SHUT_RDWR);
}

void http_loop(void)
{
	char go_away[] = "HTTP/1.1 403 Forbidden\n";
	char pending[] = "HTTP/1.1 202 Accepted\n";
//	char welcome[] = "HTTP/1.1 200 OK\n";
	char overload[] = "HTTP/1.1 503 Service Unavailable\n";

	int ret;
	fd_set set;
	FD_ZERO(&set);
	FD_SET(s, &set);
	struct timeval timeout;
	timeout.tv_sec = 0;
	timeout.tv_usec = 0;

	while(1)
	{
		ret = select(s+1, &set, NULL, NULL, &timeout);
		if(-1 == ret)
		{
#ifdef _WIN32
			log_fatal("select(), WSAGetLastError() = %d\n", WSAGetLastError());

#else
			log_fatal("select() = %d", ret);
#endif
			return;
		}
		if(!ret)return; // no pending requests

		struct sockaddr_in addr;
		socklen_t asize = sizeof(struct sockaddr_in);

		int client;
		client = accept(s, (struct sockaddr*)&addr, &asize);
	/*
		for(int i=0; i< http_client_num; i++)
		{
			if(http_clients[i].addr.sin_addr.s_addr == addr.sin_addr.s_addr)
			{
				// this is an authorised client
				log_debug("welcome back");
				break;
			}
		}
	*/

		char buf[3000];
		memset(buf, 0, 3000);
		
	//	inet_ntop(addr.sin_family, &addr.sin_addr, buf, 3000);

	//	log_debug("%s - ", buf);

		memset(buf, 0, 3000);
		ret = read(client, buf, 3000);
		if(-1 == ret)
		{
			log_fatal("read()");
			return;
		}

		int auth_ok = 0;

		for(int i=0; i<http_num_auth; i++)
		{
			if(addr.sin_family == http_authorised_clients[i].addr.sin_family)
			if(addr.sin_addr.s_addr ==
				http_authorised_clients[i].addr.sin_addr.s_addr)
			{
				// This client has been authorised, give them what they want
				auth_ok = 1;
				break;
			}
		}


		if(!auth_ok)
		{
			int found = 0;
			for(int i=0; i<http_num_pending; i++)
			{
				if(addr.sin_family == http_pending_requests[i].addr.sin_family)
				if(addr.sin_addr.s_addr ==
					http_pending_requests[i].addr.sin_addr.s_addr)
				{	// if you have an existing pending request...
					found = 1;
					if(!strncmp("GET /favicon.ico HTTP/1.1", buf, 25))
					{	// this better be a favicon req
						http_pending_requests[i].favicon = client;
						write(client, pending, strlen(pending));
					}
					else	// go away
					{
						write(client, go_away, strlen(go_away));
						close(client);
					}
				}
			}

			if(!found)
			{
				if(http_num_pending >= HTTP_MAX_CLIENTS)
				{
					write(client, overload, strlen(overload));
					close(client);
				}
				else
				{	// just give me your paperwork...
					http_pending_requests[http_num_pending].addr = addr;
					http_pending_requests[http_num_pending].request = client;
					http_pending_requests[http_num_pending].favicon = 0;
					http_pending_requests[http_num_pending].authorised = 0;

					// get the requested URL
					memset(http_pending_requests[http_num_pending].get, 0, MAX_LEN);
					int getlen = strstr(buf+4, "\n") - (buf+4);
					if(getlen > MAX_LEN) getlen = MAX_LEN;
					memcpy(http_pending_requests[http_num_pending].get, buf+4,
						getlen);

					int papers_in_order = 0;
					// get the reported client name
					char* name_off = strstr(buf, "(");
					char* name_end = strstr(name_off, ")");
					int name_len = name_end - name_off;
					int i;
					for(i=name_len; i>0; i--)
					{
						if(';' == name_off[i])
						{
							i += 2;
							break;
						}
					}
					name_len -= i;
					if(name_len > MAX_LEN)name_len = MAX_LEN;
					if(name_len > 5)papers_in_order = 1;

					memset(http_pending_requests[http_num_pending].name, 0,
						MAX_LEN);
					memcpy(http_pending_requests[http_num_pending].name,
						&name_off[i], name_len);
					http_pending_requests[http_num_pending].name[name_len]=0;

					if(papers_in_order)
					{	// take a seat...
						write(client, pending, strlen(pending));
						http_num_pending++;
					}
					else
					{	// the door is that way
						write(client, go_away, strlen(go_away));
						close(client);
					}
				}
			}	// if(!found)
		}	// if(auth_ok)
	}	// while(1)
}



char * http_get_name_local(void)
{
	return "127.0.0.1:20000";
}


char * http_get_name_auth(int index)
{
	if(index >= http_num_auth)return NULL;
	return http_authorised_clients[index].name;
}


char * http_get_name_pending(int index)
{
	if(index >= http_num_pending)return NULL;
	return http_pending_requests[index].name;
}


