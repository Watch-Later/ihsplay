#pragma once

#include <stdbool.h>
#include <SDL.h>

#include "ihslib.h"
#include "settings/app_settings.h"

typedef struct app_ui_t app_ui_t;
typedef struct stream_manager_t stream_manager_t;
typedef struct host_manager_t host_manager_t;

typedef struct app_t {
    bool running;
    app_ui_t *ui;
    app_settings_t settings;
    IHS_ClientConfig client_config;
    host_manager_t *hosts_manager;
    stream_manager_t *stream_manager;
} app_t;

typedef enum app_event_type_t {
    APP_EVENT_BEGIN = SDL_USEREVENT,
    APP_RUN_ON_MAIN,
    APP_EVENT_SIZE = (APP_RUN_ON_MAIN - APP_EVENT_BEGIN) + 1
} app_event_type_t;

typedef void(*app_run_action_fn)(app_t *, void *);

app_t *app_create(void *disp);

void app_destroy(app_t *app);

void app_quit(app_t *app);

void app_run_on_main(app_t *app, app_run_action_fn action, void *data);

void app_run_on_main_sync(app_t *app, app_run_action_fn action, void *data);

void app_ihs_log(IHS_LogLevel level, const char *tag, const char *message);