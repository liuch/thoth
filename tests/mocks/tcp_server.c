#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "tcp_server.h"
#include "../../src/common.h"

static struct buffer query = { 0, NULL };
static struct buffer response = { 0, NULL };

static unsigned int server_port = 0;
static int server_socket;
static struct sockaddr_in server_address;
static short int server_started = 0;

static pthread_t t_id;
static pthread_attr_t t_attr;

static void free_buffers(void);

static void *listener(void *data);

void tcpserver_start(const char *resp, size_t rsize)
{
	free_buffers();
	server_started = 1;

	if (rsize == 0) {
		rsize = strlen(resp);
	}
	response.pointer = malloc(rsize);
	if (response.pointer == NULL) {
		fprintf(stderr, "Error: Out of memory\n");
		abort();
	}
	response.size = rsize;
	memcpy(response.pointer, resp, rsize);

	server_socket = socket(AF_INET, SOCK_STREAM, 0);

	memset(&server_address, 0, sizeof(server_address));
	server_address.sin_family = AF_INET;
	server_address.sin_port = 0;
	inet_pton(AF_INET, "127.0.0.1", &server_address.sin_addr);
	bind(server_socket, (struct sockaddr *)&server_address, sizeof(server_address));

	socklen_t len = sizeof(server_address);
	getsockname(server_socket, (struct sockaddr *)&server_address, &len);
	server_port = ntohs(server_address.sin_port);

	if (listen(server_socket, 5) == -1) {
		fprintf(stderr, "Error: Cannot listen the socket\n");
		abort();
	}

	pthread_attr_init(&t_attr);
	if (pthread_create(&t_id, &t_attr, listener, NULL) != 0) {
		fprintf(stderr, "Error: Fail to create a thread\n");
		abort();
	}
}

int tcpserver_get_port(void)
{
	return server_port;
}

const char *tcpserver_get_query(size_t *plen)
{
	if (plen != NULL) *plen = query.size;
	return query.pointer;
}

void tcpserver_stop(void)
{
	if (pthread_join(t_id, NULL) != 0) {
		fprintf(stderr, "Error: Cannot join to thread\n");
		abort();
	}
	close(server_socket);
	server_started = 0;
}

void tcpserver_destroy(void)
{
	if (server_started) tcpserver_stop();
	free_buffers();
}

static void free_buffers(void)
{
	free_buffer(&query);
	free_buffer(&response);
}

static void *listener(void *data)
{
	int client_socket = accept(server_socket, NULL, NULL);
	if (client_socket == -1) {
		fprintf(stderr, "Error: Cannot accept socket\n");
		abort();
	}

	ssize_t cnt = 0;
	do {
		char *p = realloc(query.pointer, query.size + 4096);
		if (p == NULL) {
			fprintf(stderr, "Error: Out of memory\n");
			abort();
		}
		query.pointer = p;
		ssize_t cnt = recv(client_socket, query.pointer + query.size, 4096, 0);
		if (cnt == -1) {
			fprintf(stderr, "Error: Failed to read from socket\n");
			abort();
		}
		query.size += cnt;
	} while (cnt != 0);

	cnt = send(client_socket, response.pointer, response.size, 0);
	if (cnt == -1) {
		fprintf(stderr, "Error: Failed to send data to socket\n");
		abort();
	}
	if (cnt != response.size) {
		fprintf(stderr, "Error: Invalid number of bytes sent\n");
		abort();
	}

	close(client_socket);

	pthread_exit(0);
}
