#ifndef NM_ACTION_H
#define NM_ACTION_H
#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    NM_ACTION_RESULT_TYPE_SILENT = 0,
    NM_ACTION_RESULT_TYPE_MSG    = 1,
    NM_ACTION_RESULT_TYPE_TOAST  = 2,
    NM_ACTION_RESULT_TYPE_SKIP   = 3, // for use by skip only
} nm_action_result_type_t;

typedef struct {
    nm_action_result_type_t type;
    char *msg;
    int skip; // for use by skip only
} nm_action_result_t;

// nm_action_fn_t represents an action. On success, a nm_action_result_t is
// returned and needs to be freed with nm_action_result_free. Otherwise, NULL is
// returned and nm_err is set.
typedef nm_action_result_t *(*nm_action_fn_t)(const char *arg);

nm_action_result_t *nm_action_result_silent();
nm_action_result_t *nm_action_result_msg(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
nm_action_result_t *nm_action_result_toast(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
void nm_action_result_free(nm_action_result_t *res);

#define NM_ACTION(name) nm_action_##name

#ifdef __cplusplus
#define NM_ACTION_(name) extern "C" nm_action_result_t *NM_ACTION(name)(const char *arg)
#else
#define NM_ACTION_(name) nm_action_result_t *NM_ACTION(name)(const char *arg)
#endif

// Keep the upstream Qt 5 build as the default. Qt 6 builds must set this for
// both C and C++ sources; Qt headers are not available in the C action registry.
#ifndef NM_QT_MAJOR
#define NM_QT_MAJOR 5
#endif
#if NM_QT_MAJOR != 5 && NM_QT_MAJOR != 6
#error Unsupported NM_QT_MAJOR
#endif

// X(name, qt5, qt6). These flags enable implementations; they do
// not promise that every subaction, minor firmware, or device is supported.
#define NM_ACTIONS               \
    X(cmd_spawn,          1, 1)   \
    X(cmd_output,         1, 1)   \
    X(dbg_syslog,         1, 1)   \
    X(dbg_error,          1, 1)   \
    X(dbg_msg,            1, 1)   \
    X(dbg_toast,          1, 1)   \
    X(kfmon,              1, 1)   \
    X(kfmon_id,           1, 1)   \
    X(nickel_setting,     1, 1)   \
    X(nickel_extras,      1, 0)   \
    X(nickel_browser,     1, 1)   \
    X(nickel_misc,        1, 1)   \
    X(nickel_open,        1, 1)   \
    X(nickel_wifi,        1, 1)   \
    X(nickel_bluetooth,   1, 0)   \
    X(nickel_orientation, 1, 1)   \
    X(nickel_screenshot,  1, 1)   \
    X(power,              1, 1)   \
    X(skip,               1, 1)   \
    X(uninstall,          1, 1)

#define X(name, qt5, qt6) NM_ACTION_(name);
NM_ACTIONS
#undef X

#define X(name, qt5, qt6) enum { nm_action_support_##name = (qt5 << 5) | (qt6 << 6) };
NM_ACTIONS
#undef X
#define NM_ACTION_SUPPORTS(name, major) ((nm_action_support_##name & (1 << (major))) != 0)

#if NM_QT_MAJOR == 5
#define NM_ACTION_SUPPORTED(qt5, qt6) qt5
#else
#define NM_ACTION_SUPPORTED(qt5, qt6) qt6
#endif

#ifdef __cplusplus
}
#endif
#endif
