#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#else
#include <time.h>
#endif

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "finwo/http-parser.h"
#include "finwo/fnet.h"

#include "http-server.h"

static void sleep_ms(long ms) {
#if defined(__APPLE__)
    usleep(ms * 1000);
#elif defined(_WIN32) || defined(_WIN64)
    Sleep(ms);
#else
    time_t sec = (int)(ms / 1000);
    const long t = ms -(sec * 1000);
    struct timespec req;
    req.tv_sec = sec;
    req.tv_nsec = t * 1000000L;
    while(-1 == nanosleep(&req, &req));
#endif
}

struct fnet_udata {
  struct http_server_opts *opts;
  struct fnet_options_t   *fnet_opts;
};

struct hs_route {
  void *next;
  const char *method;
  const char *path;
  void (*fn)(struct http_server_reqdata*);
};

struct hs_route *registered_routes = NULL;

void _hs_onServing(struct fnet_ev *ev) {
  struct fnet_udata *ludata = ev->udata;

  if (ludata->opts->evs && ludata->opts->evs->serving) {
    ludata->opts->evs->serving(ludata->opts->addr, ludata->opts->port, ludata->opts->udata);
  }
}

void _hs_onTick(struct fnet_ev *ev) {
  struct fnet_udata *ludata = ev->udata;

  if (ludata->opts->evs && ludata->opts->evs->tick) {
    ludata->opts->evs->tick(ludata->opts->udata);
  }
}

static void _hs_onRequest(struct http_parser_event *ev) {
  struct http_server_reqdata *reqdata = ev->udata;
  struct hs_route *route              = registered_routes;
  struct hs_route *selected_route     = NULL;

  // Method/path matching, should be more intricate later (like /posts/:postId/comments)
  while(route) {
    if (
      (!strcmp(ev->request->method, route->method)) &&
      (!strcmp(ev->request->path  , route->path  ))
    ) {
      selected_route = route;
    }
    route = route->next;
  }

  if (!selected_route) {
    if (reqdata->evs && reqdata->evs->notFound) {
      reqdata->evs->notFound(reqdata);
      return;
    } else {
      fnet_close(reqdata->connection);
      return;
    }
  }

  // Call the route handler
  selected_route->fn(reqdata);
}

void _hs_onData(struct fnet_ev *ev) {
  struct http_server_reqdata *reqdata = ev->udata;
  http_parser_pair_request_data(reqdata->reqres, ev->buffer);
}

void _hs_onClose(struct fnet_ev *ev) {
  struct http_server_reqdata *reqdata = ev->udata;

  if (reqdata->evs && reqdata->evs->close) {
    reqdata->evs->close(reqdata);
  }

  http_parser_pair_free(reqdata->reqres);
  free(reqdata);
}

void _hs_onConnect(struct fnet_ev *ev) {
  struct fnet_t     *conn   = ev->connection;
  struct fnet_udata *ludata = ev->udata;

  // Setup new request/response pair
  struct http_server_reqdata *reqdata = malloc(sizeof(struct http_server_reqdata));
  reqdata->connection        = conn;
  reqdata->reqres            = http_parser_pair_init(reqdata);
  reqdata->reqres->onRequest = _hs_onRequest;
  reqdata->evs               = ludata->opts->evs;
  ev->connection->udata      = reqdata;

  // Setup data flowing from connection into reqres
  ev->connection->onData  = _hs_onData;
  ev->connection->onClose = _hs_onClose;
}

void http_server_response_send(struct http_server_reqdata *reqdata, bool close) {
  struct buf *response_buffer = http_parser_sprint_response(reqdata->reqres->response);
  fnet_write(reqdata->connection, response_buffer);
  buf_clear(response_buffer);
  free(response_buffer);
  if (close) {
    fnet_close(reqdata->connection);
  }
}

void http_server_route(const char *method, const char *path, void (*fn)(struct http_server_reqdata*)) {
  struct hs_route *route = malloc(sizeof(struct hs_route));
  route->next   = registered_routes;
  route->method = method;
  route->path   = path;
  route->fn     = fn;
  registered_routes = route;
}

void _hs_onListenClose(struct fnet_ev *ev) {
  struct fnet_udata *ludata = ev->udata;
  if (!ludata->opts->shutdown) {
    ludata->opts->listen_connection = fnet_listen(ludata->opts->addr, ludata->opts->port, ludata->fnet_opts);
  }
}

void http_server_main(struct http_server_opts *opts) {
  int ret;
  if (!opts) exit(1);
  opts->shutdown = false;

  // Prepare http context
  struct fnet_udata *ludata = calloc(1, sizeof(struct fnet_udata));
  ludata->opts = opts;

  // Prepare network options
  struct fnet_options_t fnet_opts = {
    .proto     = FNET_PROTO_TCP,
    .flags     = 0,
    .onListen  = _hs_onServing,
    .onConnect = _hs_onConnect,
    .onData    = NULL,
    .onTick    = _hs_onTick,
    .onClose   = _hs_onListenClose,
    .udata     = ludata,
  };

  // Track network options in http context
  ludata->fnet_opts   = &fnet_opts;

  // Signal that we want our port
  ludata->opts->listen_connection = fnet_listen(ludata->opts->addr, ludata->opts->port, ludata->fnet_opts);
  if (!(ludata->opts->listen_connection)) {
    exit(1);
  }

  // This is a forever function, controlled by network thread
  while(!opts->shutdown) {
    printf("(re)starting fnet_main\n");
    ret = fnet_main();
    printf("fnet_main returned %d\n", ret);
    if (ret) exit(ret);
    sleep_ms(100);
  }
}
