#include <assert.h>
#include <stddef.h>
#include <string.h>
#include "action.h"
#include "util.h"

// Supply only enabled implementations. The production action.c must supply
// exactly the remaining functions, with the same C linkage and error contract.
#define IMPL_0(name)
#define IMPL_1(name) NM_ACTION_(name) { (void)arg; return nm_action_result_silent(); }
#define IMPL_(name, supported) IMPL_##supported(name)
#define IMPL(name, supported) IMPL_(name, supported)
#define X(name, qt5, qt6) IMPL(name, NM_ACTION_SUPPORTED(qt5, qt6))
NM_ACTIONS
#undef X

int main(void) {
    int count = 0, unsupported = 0;
    assert(NM_ACTION_SUPPORTS(nickel_extras, 5));
    assert(!NM_ACTION_SUPPORTS(nickel_extras, 6));
    assert(NM_ACTION_SUPPORTS(nickel_bluetooth, 5));
    assert(!NM_ACTION_SUPPORTS(nickel_bluetooth, 6));
    assert(NM_ACTION_SUPPORTS(nickel_screenshot, 5));
    assert(NM_ACTION_SUPPORTS(nickel_screenshot, 6));
#define X(name, qt5, qt6) do { \
    ++count; \
    nm_err_set(NULL); \
    nm_action_result_t *res = NM_ACTION(name)("test"); \
    if (NM_ACTION_SUPPORTS(name, NM_QT_MAJOR)) { \
        assert(res); \
        assert(!nm_err()); \
        nm_action_result_free(res); \
    } else { \
        ++unsupported; \
        assert(!res); \
        const char *err = nm_err(); \
        assert(err && strstr(err, #name) && strstr(err, "not supported on Qt 6 firmware")); \
    } \
} while (0);
    NM_ACTIONS
#undef X
    assert(count == 20);
    assert(unsupported == (NM_QT_MAJOR == 6 ? 2 : 0));
    return 0;
}
