#pragma once

#include <ihslib.h>

typedef struct app_t app_t;
typedef struct host_manager_t host_manager_t;

typedef struct stream_manager_t stream_manager_t;

typedef struct stream_manager_callbacks_t {
    void (*connected)(const IHS_SessionInfo *info, void *context);

    void (*disconnected)(const IHS_SessionInfo *info, void *context);
} stream_manager_listener_t;

stream_manager_t *stream_manager_create(app_t *app, host_manager_t *host_manager);

void stream_manager_destroy(stream_manager_t *manager);

void stream_manager_register_listener(stream_manager_t *manager, const stream_manager_listener_t *listener,
                                      void *context);

void stream_manager_unregister_listener(stream_manager_t *manager, const stream_manager_listener_t *listener);

bool stream_manager_start(stream_manager_t *manager, const IHS_HostInfo *host);

IHS_Session *stream_manager_active_session(const stream_manager_t *manager);

void stream_manager_stop_active(stream_manager_t *manager);
