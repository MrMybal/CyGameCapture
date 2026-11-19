// Copyright (C) 2026 Cyberalien
// SPDX-License-Identifier: MIT
#pragma once
#include <stddef.h>
#ifdef CYGC_AUTOMATION_BUILD
#define CYGC_MCP_API __declspec(dllexport)
#else
#define CYGC_MCP_API __declspec(dllimport)
#endif
// UTF-8 JSON C ABI: all SDK allocations remain inside the DLL. Callbacks run only in pump().
extern "C"
{
    typedef const char *(__cdecl *cygc_operation_handler)(void *, const char *, const char *);
    CYGC_MCP_API void *cygc_automation_create(const char *options_json);
    CYGC_MCP_API int cygc_automation_add(void *, const char *operation_json, cygc_operation_handler, void *);
    CYGC_MCP_API const char *cygc_automation_connection(void *);
    CYGC_MCP_API const char *cygc_automation_error();
    CYGC_MCP_API void cygc_automation_pump(void *);
    CYGC_MCP_API void cygc_automation_allow_mutations(void *, int allow);
    CYGC_MCP_API void cygc_automation_destroy(void *);
    CYGC_MCP_API int cygc_automation_chat_send(void *, const char *request_json);
    CYGC_MCP_API const char *cygc_automation_chat_events(void *);
    CYGC_MCP_API void cygc_automation_chat_cancel(void *);
    CYGC_MCP_API void cygc_automation_chat_clear(void *);
    CYGC_MCP_API const char *cygc_automation_chat_history(void *);
}
