#ifndef __FINWO_HTTP_SERVER_H__
#define __FINWO_HTTP_SERVER_H__

#include <stdbool.h>
#include <stdint.h>

struct hs_udata {
  struct evio_conn *connection;    // From the underlaying connection library
  struct http_parser_pair *reqres; // The request/response pair
  void *info;                      // See C file for definition, type not exported
};

struct http_server_events {
  int64_t (*tick)(void *udata);
  void (*serving)(const char **addrs, int naddrs, void *udata);
  void (*error)(const char *message, bool fatal, void *udata);
  void (*close)(struct hs_udata *conn, void *udata);
  void (*notFound)(struct hs_udata*);
};

void http_server_response_send(struct hs_udata *hsdata, bool close);
void http_server_route(char *method, char *path, void (*fn)(struct hs_udata*));
void http_server_main(char **addrs, int naddrs, struct http_server_events *hsevs, void *udata);

#endif // __FINWO_HTTP_SERVER_H__
