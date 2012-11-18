//libwebsock Copyright 2012 Payden Sutherland
#include <openssl/ssl.h>
#include <event2/event.h>
#include <event2/buffer.h>
#include <event2/bufferevent.h>
#include <event2/bufferevent_ssl.h>


#define PORT_STRLEN 12
#define LISTEN_BACKLOG 10
#define FRAME_CHUNK_LENGTH 1024
#define MASK_LENGTH 4


#define STATE_SHOULD_CLOSE (1 << 0)
#define STATE_SENT_CLOSE_FRAME (1 << 1)
#define STATE_CONNECTING (1 << 2)
#define STATE_IS_SSL (1 << 3)
#define STATE_CONNECTED (1 << 4)


typedef struct _libwebsock_string {
	char *data;
	int length;
	int idx;
	int data_sz;
} libwebsock_string;

typedef struct _libwebsock_frame {
	unsigned int fin;
	unsigned int opcode;
	unsigned int mask_offset;
	unsigned int payload_offset;
	unsigned int rawdata_idx;
	unsigned int rawdata_sz;
	unsigned long long payload_len;
	char *rawdata;
	struct _libwebsock_frame *next_frame;
	struct _libwebsock_frame *prev_frame;
	unsigned char mask[4];
} libwebsock_frame;

typedef struct _libwebsock_message {
	unsigned int opcode;
	unsigned long long payload_len;
	char *payload;
} libwebsock_message;

typedef struct _libwebsock_client_state {
	int sockfd;
	int flags;
	void *data;
	libwebsock_frame *current_frame;
	struct sockaddr_storage *sa;
	SSL *ssl;
	struct bufferevent *bev;
	int (*onmessage)(struct _libwebsock_client_state*, libwebsock_message*);
	int (*control_callback)(struct _libwebsock_client_state*, libwebsock_frame*);
	int (*onopen)(struct _libwebsock_client_state*);
	int (*onclose)(struct _libwebsock_client_state*);
} libwebsock_client_state;

typedef struct _libwebsock_context {
	int running;
	int ssl_init;
	struct event_base *base;
	int (*onmessage)(libwebsock_client_state*, libwebsock_message*);
	int (*control_callback)(libwebsock_client_state*, libwebsock_frame*);
	int (*onopen)(libwebsock_client_state*);
	int (*onclose)(libwebsock_client_state*);
} libwebsock_context;

typedef struct _libwebsock_ssl_event_data {
	SSL_CTX *ssl_ctx;
	libwebsock_context *ctx;
} libwebsock_ssl_event_data;



//function defs

int libwebsock_send_binary(libwebsock_client_state *state, char *in_data, unsigned long long datalen);
int libwebsock_send_text(libwebsock_client_state *state, char *strdata);
int libwebsock_complete_frame(libwebsock_frame *frame);
int libwebsock_default_onclose_callback(libwebsock_client_state *state);
int libwebsock_default_onopen_callback(libwebsock_client_state *state);
int libwebsock_default_onmessage_callback(libwebsock_client_state *state, libwebsock_message *msg);
int libwebsock_default_control_callback(libwebsock_client_state *state, libwebsock_frame *ctl_frame);
void libwebsock_fail_connection(libwebsock_client_state *state);
void libwebsock_cleanup_context(libwebsock_context *ctx);
void libwebsock_handle_signal(evutil_socket_t sig, short event, void *ptr);
void libwebsock_handle_control_frame(libwebsock_client_state *state, libwebsock_frame *ctl_frame);
void libwebsock_dispatch_message(libwebsock_client_state *state, libwebsock_frame *current);
void libwebsock_dump_frame(libwebsock_frame *frame);
void libwebsock_handle_accept_ssl(evutil_socket_t listener, short event, void *arg);
void libwebsock_handle_accept(evutil_socket_t listener, short event, void *arg);
void libwebsock_handle_recv(struct bufferevent *bev, void *ptr);
void libwebsock_handle_client_event(libwebsock_context *ctx, libwebsock_client_state *state);
void libwebsock_do_read(struct bufferevent *bev, void *ptr);
void libwebsock_do_error(struct bufferevent *bev, short error, void *ptr);
void libwebsock_wait(libwebsock_context *ctx);
void libwebsock_handshake_finish(struct bufferevent *bev, libwebsock_client_state *state);
void libwebsock_handshake(struct bufferevent *bev, void *ptr);
void libwebsock_bind(libwebsock_context *ctx, char *listen_host, char *port);
void libwebsock_bind_ssl(libwebsock_context *ctx, char *listen_host, char *port, char *keyfile, char *certfile);
void libwebsock_bind_ssl_real(libwebsock_context *ctx, char *listen_host, char *port, char *keyfile, char *certfile, char *chainfile);
libwebsock_context *libwebsock_init(void);
