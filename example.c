#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "finwo/fnet.h"
#include "finwo/http-parser.h"
#include "pierreguillot/thread.h"
#include "tidwall/buf.h"


#include "http-server.h"

uint16_t targetPort = 8080;

void onServing(char *addr, uint16_t port, void *udata) {
  printf("Serving at %s:%d\n", addr, port);
}

const int countDownOrg = 60;
int       countDown    = countDownOrg;
void onTick(void *udata) {
  struct http_server_opts *opts = udata;

  // Handle auto-shutdown
  printf("Shutdown in %d second(s)\n", --countDown);
  if (countDown <= 0) {
    opts->shutdown = true;
    return;
  }

  // Handle port re-assign
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
  countDown = countDownOrg;
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

  // Launch network management thread
  thd_thread thread;
  thd_thread_detach(&thread, http_server_fnet_thread, NULL);

  http_server_main(&opts);
  fnet_shutdown();

  thd_thread_join(&thread);

  printf("Server has shut down\n");
}
