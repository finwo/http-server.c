#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "finwo/http-parser.h"
#include "tidwall/evio.h"

#include "http-server.h"

struct evio_udata {
  struct http_server_events *hsevs;
  void *udata;
};

struct hs_route {
  void *next;
  char *method;
  char *path;
  void (*fn)(struct hs_udata*);
};

struct hs_route *registered_routes = NULL;

void _hs_onServing(const char **addrs, int naddrs, void *udata) {
  struct evio_udata *info = udata;
  if (info->hsevs->serving) {
    info->hsevs->serving(addrs, naddrs, info->udata);
  }
}

void _hs_onError(const char *message, bool fatal, void *udata) {
  struct evio_udata *info = udata;
  if (info->hsevs->error) {
    info->hsevs->error(message, fatal, info->udata);
  }
}

int64_t _hs_onTick(void *udata) {
  struct evio_udata *info = udata;
  if (info->hsevs->tick) {
    return info->hsevs->tick(info->udata);
  }
  return 1e9; // 1 second
}

static void _hs_onRequest(struct http_parser_event *ev) {
  struct hs_udata *hsdata         = ev->udata;
  struct evio_udata *info         = hsdata->info;
  struct hs_route *route          = registered_routes;
  struct hs_route *selected_route = NULL;

  // Method/path matching, should be more intricate later
  while(route) {
    if (
      (!strcmp(ev->request->method, route->method)) &&
      (!strcmp(ev->request->path  , route->path  ))
    ) {
      selected_route = route;
    }
    route = route->next;
  }

  // No 404 handler (yet)
  if (!selected_route) {
    if (info->hsevs->notFound) {
      info->hsevs->notFound(hsdata);
      return;
    } else {
      evio_conn_close(hsdata->connection);
      return;
    }
  }

  // Call the route handler
  selected_route->fn(hsdata);
}


void _hs_onOpen(struct evio_conn *conn, void *udata) {
  struct evio_udata *info = udata;
  struct hs_udata *hsdata = malloc(sizeof(struct hs_udata));
  hsdata->connection = conn;
  hsdata->info       = info;
  hsdata->reqres     = http_parser_pair_init(hsdata);
  hsdata->reqres->onRequest = _hs_onRequest;
  evio_conn_set_udata(conn, hsdata);
}

void _hs_onClose(struct evio_conn *conn, void *udata) {
  struct evio_udata *info   = udata;
  struct hs_udata *hsdata = evio_conn_udata(conn);

  if (info->hsevs->close) {
    info->hsevs->close(hsdata, info->udata);
  }

  http_parser_pair_free(hsdata->reqres);
  free(hsdata);
}

void _hs_onData(struct evio_conn *conn, const void *data, size_t len, void *udata) {
  struct hs_udata *hsdata = evio_conn_udata(conn);
  http_parser_pair_request_data(hsdata->reqres, data, len);
}

void http_server_response_send(struct hs_udata *hsdata, bool close) {
  char *response_buffer = http_parser_sprint_response(hsdata->reqres->response);
  evio_conn_write(hsdata->connection, response_buffer, strlen(response_buffer));
  free(response_buffer);
  if (close) {
    evio_conn_close(hsdata->connection);
  }
}

void http_server_route(char *method, char *path, void (*fn)(struct hs_udata*)) {
  struct hs_route *route = calloc(1, sizeof(struct hs_route));
  route->next   = registered_routes;
  route->method = method;
  route->path   = path;
  route->fn     = fn;
  registered_routes = route;
}

void http_server_main(char **addrs, int naddrs, struct http_server_events *hsevs, void *udata) {
  struct evio_udata *info = calloc(1, sizeof(struct evio_udata));
  info->hsevs  = hsevs;
  info->udata  = udata;

  struct evio_events evs  = {
    .serving = _hs_onServing,
    .error   = _hs_onError,
    .tick    = _hs_onTick,
    .opened  = _hs_onOpen,
    .closed  = _hs_onClose,
    .data    = _hs_onData,
  };

  // This is a forever function
  evio_main(addrs, naddrs, evs, info);
}
