#define _GNU_SOURCE // vasprintf
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "action.h"
#include "util.h"

nm_action_result_t *nm_action_result_silent() {
    nm_action_result_t *res = calloc(1, sizeof(nm_action_result_t));
    res->type = NM_ACTION_RESULT_TYPE_SILENT;
    return res;
}

#define _nm_action_result_fmt(_fn, _typ)                                 \
    nm_action_result_t *nm_action_result_##_fn(const char *fmt, ...) {   \
        nm_action_result_t *res = calloc(1, sizeof(nm_action_result_t)); \
        res->type = _typ;                                                \
        va_list v;                                                       \
        va_start(v, fmt);                                                \
        if (vasprintf(&res->msg, fmt, v) == -1)                          \
            res->msg = strdup("error");                                  \
        va_end(v);                                                       \
        return res;                                                      \
    }

_nm_action_result_fmt(msg,    NM_ACTION_RESULT_TYPE_MSG);
_nm_action_result_fmt(toast,  NM_ACTION_RESULT_TYPE_TOAST);

void nm_action_result_free(nm_action_result_t *res) {
    if (!res)
        return;
    if (res->msg)
        free(res->msg);
    free(res);
}

// Keep unsupported actions in the config parser so chain_failure can recover.
#define NM_ACTION_STUB_0(name) NM_ACTION_(name) { \
    (void)arg; \
    NM_ERR_RET(NULL, "action '%s' is not supported on Qt 6 firmware", #name); \
}
#define NM_ACTION_STUB_1(name)
#define NM_ACTION_STUB_(name, supported) NM_ACTION_STUB_##supported(name)
#define NM_ACTION_STUB(name, supported) NM_ACTION_STUB_(name, supported)
#define X(name, qt5, qt6) NM_ACTION_STUB(name, NM_ACTION_SUPPORTED(qt5, qt6))
NM_ACTIONS
#undef X
