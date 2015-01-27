/****************************************************************************************
**
** Copyright (C) 2015 Jolla Ltd.
** Contact: Thomas Perl <thomas.perl@jolla.com>
** All rights reserved.
**
** This file is part of nemo keepalive package.
**
** You may use this file under the terms of the GNU Lesser General
** Public License version 2.1 as published by the Free Software Foundation
** and appearing in the file license.lgpl included in the packaging
** of this file.
**
** This library is free software; you can redistribute it and/or
** modify it under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation
** and appearing in the file license.lgpl included in the packaging
** of this file.
**
** This library is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
** Lesser General Public License for more details.
**
****************************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <dbus/dbus.h>
#include <dbus/dbus-glib-lowlevel.h>
#include "keepalive-cpukeepalive.h"

#define failure(FMT, ARGS...) do {\
    fprintf(stderr, "%s: "FMT"\n", __FUNCTION__, ## ARGS);\
    exit(EXIT_FAILURE); \
} while(0)


struct KeepaliveOptions {
    gint timeout;
};

struct Keepalive {
    struct KeepaliveOptions *options;
    DBusConnection *system_bus;
    GMainLoop *mainloop_handle;
    GPid pid;
    cpukeepalive_t *cpukeepalive;
    gint result;
    guint timeout_source_id;
};


static void watch_child(GPid pid, gint status, gpointer user_data)
{
    struct Keepalive *keepalive = user_data;
    keepalive->result = status;
    g_main_loop_quit(keepalive->mainloop_handle);
}

static gboolean on_timeout(gpointer user_data)
{
    struct Keepalive *keepalive = user_data;
    cpukeepalive_stop(keepalive->cpukeepalive);
    keepalive->timeout_source_id = 0;
    return FALSE;
}

static struct Keepalive *keepalive_new(char **argv, struct KeepaliveOptions *options)
{
    struct Keepalive *keepalive = g_new0(struct Keepalive, 1);

    keepalive->mainloop_handle = g_main_loop_new(0, 0);

    DBusError error = DBUS_ERROR_INIT;
    keepalive->system_bus = dbus_bus_get(DBUS_BUS_SYSTEM, &error);
    if (!keepalive->system_bus) {
        failure("%s: %s", error.name, error.message);
    }
    dbus_connection_setup_with_g_main(keepalive->system_bus, 0);
    dbus_error_free(&error);

    keepalive->cpukeepalive = cpukeepalive_new();
    cpukeepalive_start(keepalive->cpukeepalive);

    keepalive->options = options;

    GError *err = NULL;
    if (!g_spawn_async(NULL, argv, NULL, G_SPAWN_DO_NOT_REAP_CHILD |
                                           G_SPAWN_SEARCH_PATH |
                                           G_SPAWN_CHILD_INHERITS_STDIN,
                       NULL, NULL, &keepalive->pid, &err)) {
        failure("Could not exec child: %s", err->message);
    }
    g_clear_error(&err);

    if (options->timeout) {
        keepalive->timeout_source_id = g_timeout_add(options->timeout * 1000, on_timeout, keepalive);
    }

    g_child_watch_add(keepalive->pid, watch_child, keepalive);

    return keepalive;
}

static struct Keepalive *keepalive_run(struct Keepalive *keepalive)
{
    g_main_loop_run(keepalive->mainloop_handle);
    cpukeepalive_stop(keepalive->cpukeepalive);
    return keepalive;
}

static int keepalive_free(struct Keepalive *keepalive)
{
    if (keepalive->timeout_source_id) {
        g_source_remove(keepalive->timeout_source_id);
    }

    if (keepalive->cpukeepalive) {
        cpukeepalive_unref(keepalive->cpukeepalive);
    }

    if (keepalive->system_bus) {
        dbus_connection_unref(keepalive->system_bus);
    }

    if (keepalive->mainloop_handle) {
        g_main_loop_unref(keepalive->mainloop_handle);
    }

    int result = keepalive->result;
    g_free(keepalive);
    return result;
}


int main(int argc, char **argv)
{
    struct KeepaliveOptions options = {
        0, // timeout
    };

    GOptionEntry entries[] = {
        { "timeout", 't', 0, G_OPTION_ARG_INT, &options.timeout,
            "Maximum time to keep the CPU alive", "SECONDS", },
        { 0, 0, 0, 0, 0, 0, 0 },
    };

    GOptionContext *ctx = g_option_context_new("COMMAND [ARGUMENTS...]");
    g_option_context_set_summary(ctx, "Enable CPU keepalive during runtime of child process");
    g_option_context_set_description(ctx, "https://github.com/nemomobile/nemo-keepalive");
    g_option_context_add_main_entries(ctx, entries, NULL);
    GError *error = 0;
    if (!g_option_context_parse(ctx, &argc, &argv, &error)) {
        failure("Option parsing: %s", error->message);
    }
    if (argc > 1 && strcmp(argv[1], "--") == 0) {
        argv++;
        argc--;
    }
    if (argc == 1) {
        fprintf(stderr, "Error: No command specified. Use -h/--help for help.\n");
        exit(1);
    }
    g_option_context_free(ctx);

    return keepalive_free(keepalive_run(keepalive_new(argv+1, &options)));
}
