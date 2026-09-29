#pragma once
#define NAPI_VERSION 8
#define NAPI_EXTERN extern
#include "include/node_api.h"
#include "include/winpty.h"
#define NAPI_LIST(X) \
 X(napi_get_cb_info) X(napi_get_value_int32) X(napi_get_value_string_utf16) \
 X(napi_get_array_length) X(napi_get_element) X(napi_create_object) \
 X(napi_create_int32) X(napi_create_string_utf16) X(napi_create_string_utf8) \
 X(napi_set_named_property) X(napi_create_function) X(napi_throw_error) \
 X(napi_get_undefined) X(napi_call_function) X(napi_create_threadsafe_function) \
 X(napi_call_threadsafe_function) X(napi_release_threadsafe_function) \
 X(napi_add_env_cleanup_hook) X(napi_typeof)
#define WINPTY_LIST(X) \
 X(winpty_config_new) X(winpty_config_free) X(winpty_config_set_initial_size) \
 X(winpty_config_set_agent_timeout) X(winpty_open) X(winpty_conin_name) \
 X(winpty_conout_name) X(winpty_agent_process) X(winpty_spawn_config_new) \
 X(winpty_spawn_config_free) X(winpty_spawn) X(winpty_set_size) X(winpty_free) \
 X(winpty_error_msg) X(winpty_error_free)
#define DECLARE_API(n) static decltype(&n) p_##n;
NAPI_LIST(DECLARE_API)
WINPTY_LIST(DECLARE_API)
