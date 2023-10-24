#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "finwo/fnet.h"
#include "finwo/http-parser.h"
#include "tidwall/buf.h"

#include "http-server.h"

uint16_t targetPort = 8080;

void onServing(char *addr, uint16_t port, void *udata) {
  printf("Serving at %s:%d\n", addr, port);
}

int ticksHad = 0;
void onTick(void *udata) {
  printf("Tick %d\n", ticksHad);
  if (++ticksHad >= 10) {
    printf("10 seconds have passed\n");
    ticksHad = 0;
  }

  struct http_server_opts *opts = udata;
  if (opts->port != targetPort) {
    opts->port = targetPort;
    fnet_close(opts->listen_connection);
  }
}

void route_get_hello(struct http_server_reqdata *reqdata) {
  http_parser_header_set(reqdata->reqres->response, "Content-Type", "text/plain");
  reqdata->reqres->response->body       = calloc(1, sizeof(struct buf));
  reqdata->reqres->response->body->data = strdup("Hello World\n");
  reqdata->reqres->response->body->len  = strlen(reqdata->reqres->response->body->data);
  http_server_response_send(reqdata, true);
  return;
}

void route_post_port(struct http_server_reqdata *reqdata) {
  http_parser_header_set(reqdata->reqres->response, "Content-Type", "text/plain");
  targetPort = atoi(reqdata->reqres->request->body->data);
  reqdata->reqres->response->body       = calloc(1, sizeof(struct buf));
  reqdata->reqres->response->body->data = strdup("OK\n");
  reqdata->reqres->response->body->len  = strlen(reqdata->reqres->response->body->data);
  http_server_response_send(reqdata, true);
  return;
}

void route_404(struct http_server_reqdata *reqdata) {
  http_parser_header_set(reqdata->reqres->response, "Content-Type", "text/plain");
  reqdata->reqres->response->status     = 404;
  reqdata->reqres->response->body       = calloc(1, sizeof(struct buf));
  reqdata->reqres->response->body->data = strdup("not found\n");
  reqdata->reqres->response->body->len  = strlen(reqdata->reqres->response->body->data);
  http_server_response_send(reqdata, true);
  return;
}

int main() {
  struct http_server_events evs = {
    .serving  = onServing,
    .close    = NULL,
    .notFound = route_404,
    .tick     = onTick,
  };
  struct http_server_opts opts = {
    .evs   = &evs,
    .addr  = "0.0.0.0",
    .port  = targetPort,
    .udata = &opts,
  };

  http_server_route("GET" , "/hello", route_get_hello);
  http_server_route("POST", "/port" , route_post_port);

  http_server_main(&opts);
}
