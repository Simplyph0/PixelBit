#pragma once
#include "Arduino.h"
typedef uint8_t u8;
enum { ESP_NOW_ROLE_COMBO = 3 };
typedef void (*esp_now_recv_cb_t)(u8*, u8*, u8);
typedef void (*esp_now_send_cb_t)(u8*, u8);
int esp_now_init(); int esp_now_deinit(); int esp_now_set_self_role(u8);
int esp_now_register_recv_cb(esp_now_recv_cb_t); int esp_now_unregister_recv_cb(); int esp_now_register_send_cb(esp_now_send_cb_t);
int esp_now_add_peer(u8*, u8, u8, u8*, u8); int esp_now_send(u8*, u8*, int);
