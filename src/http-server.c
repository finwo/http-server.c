#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#else
#include <time.h>
#include <unistd.h>
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
  char **path;
  void (*fn)(struct http_server_reqdata*);
};

struct hs_route *registered_routes = NULL;

char ** _hs_pathTokens(const char *path) {
  char **output = calloc(strlen(path), sizeof(char*));

  int token_count = 0;
  char *dupped = strdup(path);
  char *rest = dupped;
  char *token;
  while((token = strtok_r(rest, "/", &rest))) {
    output[token_count++] = strdup(token);
  }
  free(dupped);

  return output;
}

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

  // Tokenize the given path only once
  char **pathTokens = _hs_pathTokens(ev->request->path);
  char **routeTokens;
  char *tag = calloc(strlen(ev->request->path), sizeof(char));
  int i;

  // Method/path matching
  while(route) {

    // Skip route if the method doesn't match
    if (strcmp(ev->request->method, route->method)) {
      route = route->next;
      continue; // Continues while(route)
    }

    // Checking if the path matches
    routeTokens = route->path;
    i = 0;

    while((pathTokens[i] && routeTokens[i])) {
      if (routeTokens[i][0] == ':') { i++; continue; }
      if (strcmp(pathTokens[i], routeTokens[i])) {
        i = -1;
        break; // Breaks token-checking
      }
      i++;
    }

    // Content mismatch
    if (i == -1) {
      route = route->next;
      continue; // Continues while(route)
    }

    // Length mismatch
    if (pathTokens[i] || routeTokens[i]) {
      route = route->next;
      continue;
    }

    // Here = route match

    // Store url params as tag 'param:<name>' = 'path[i]
    i = 0;
    while((pathTokens[i] && routeTokens[i])) {
      if (routeTokens[i][0] != ':') { i++; continue; }
      tag[0] = '\0';
      strcat(tag, "param:");
      strcat(tag, routeTokens[i]+1);
      http_parser_tag_set(ev->request, tag, pathTokens[i]);
      i++;
    }

    selected_route = route;
    break;
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
  reqdata->udata             = ludata->opts->udata;
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
  route->fn     = fn;
  route->path   = _hs_pathTokens(path);
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
    ret = fnet_main();
    if (ret) exit(ret);
    sleep_ms(100);
  }
}
