#include <stddef.h>
#include <resources.h>
#include "embedded_resources.h"

const struct nh_resources nh_resources = {
    .config_dir = NM_CONFIG_DIR,
    .uninstall_file = NM_CONFIG_DIR "/uninstall",
    .files = nh_embedded_resources,
};
