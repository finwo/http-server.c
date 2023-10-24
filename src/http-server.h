#ifndef __FINWO_HTTP_SERVER_H__
#define __FINWO_HTTP_SERVER_H__

#include <stdbool.h>
#include <stdint.h>

struct http_server_reqdata {
  struct fnet_t             *connection; // From the underlaying connection library
  struct http_parser_pair   *reqres;     // The request/response pair
  struct http_server_events *evs;
  void                      *udata;
};

struct http_server_opts {
  struct http_server_events *evs;
  char                      *addr;
  uint16_t                  port;
  void                      *udata;
  struct fnet_t             *listen_connection;
};

struct http_server_events {
  void (*serving)(char *addrs, uint16_t port, void *udata);
  void (*close)(struct http_server_reqdata *reqdata);
  void (*notFound)(struct http_server_reqdata *reqdata);
  void (*tick)(void *udata);
};

void http_server_main(struct http_server_opts *opts);
void http_server_response_send(struct http_server_reqdata *reqdata, bool close);
void http_server_route(const char *method, const char *path, void (*fn)(struct http_server_reqdata*));

#endif // __FINWO_HTTP_SERVER_H__
