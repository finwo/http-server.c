#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "finwo/http-parser.h"
#include "tidwall/buf.h"

#include "http-server.h"

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
}

void route_get_hello(struct http_server_reqdata *reqdata) {
  http_parser_header_set(reqdata->reqres->response, "Content-Type", "text/plain");
  reqdata->reqres->response->body       = calloc(1, sizeof(struct buf));
  reqdata->reqres->response->body->data = strdup("Hello World\n");
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

  http_server_route("GET", "/hello", route_get_hello);

  http_server_main(&(const struct http_server_opts){
    .evs    = &evs,
    .addr   = "0.0.0.0",
    .port   = 8080,
  });
}
