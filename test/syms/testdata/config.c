#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "action.h"
#include "config.h"
#include "generator.h"
#include "util.h"

// Only parsing is under test. Enabled actions and generators cannot run here.
#define IMPL_0(name)
#define IMPL_1(name) NM_ACTION_(name) { (void)arg; abort(); }
#define IMPL_(name, supported) IMPL_##supported(name)
#define IMPL(name, supported) IMPL_(name, supported)
#define X(name, qt5, qt6) IMPL(name, NM_ACTION_SUPPORTED(qt5, qt6))
NM_ACTIONS
#undef X
#define X(name) NM_GENERATOR_(name) { (void)arg; (void)time_in_out; (void)sz_out; abort(); }
NM_GENERATORS
#undef X
nm_menu_item_t **nm_generator_do(nm_generator_t *gen, size_t *sz_out) {
    (void)gen; (void)sz_out; abort();
}
void nh_log(const char *fmt, ...) { (void)fmt; }

int main(void) {
    nm_config_file_t *files = nm_config_files();
    assert(!nm_err());
    nm_config_t *cfg = nm_config_parse(files);
    nm_config_files_free(files);
    const char *err = nm_err();
    if (err) { fputs(err, stderr); return 2; }
    size_t count = 0;
    nm_menu_item_t **items = nm_config_get_menu(cfg, &count);
    printf("%zu\n", count);
    for (size_t i = 0; i < count; ++i) {
        printf("%d|%s", items[i]->loc, items[i]->lbl);
        for (nm_menu_action_t *act = items[i]->action; act; act = act->next)
            printf("|%d%d", act->on_success, act->on_failure);
        puts("");
    }
    free(items);
    nm_config_free(cfg);
    return 0;
}
