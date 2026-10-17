/*
 * Copyright (C) 2026 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stddef.h>

#define DSI_ERROR (-1)

typedef void *dsi_hndl_t;
typedef void (*dsi_net_ev_cb)(dsi_hndl_t hndl, void *user_data, int evt, void *payload);

int dsi_init(int mode) {
    (void) mode;
    return DSI_ERROR;
}

dsi_hndl_t dsi_get_data_srvc_hndl(dsi_net_ev_cb cb, void *user_data) {
    (void) cb;
    (void) user_data;
    return NULL;
}

void dsi_rel_data_srvc_hndl(dsi_hndl_t hndl) {
    (void) hndl;
}

int dsi_set_data_call_param(dsi_hndl_t hndl, int identifier, void *info) {
    (void) hndl;
    (void) identifier;
    (void) info;
    return DSI_ERROR;
}

int dsi_start_data_call(dsi_hndl_t hndl) {
    (void) hndl;
    return DSI_ERROR;
}

int dsi_stop_data_call(dsi_hndl_t hndl) {
    (void) hndl;
    return DSI_ERROR;
}
