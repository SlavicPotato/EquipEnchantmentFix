#pragma once

#include "macro_helpers.h"
#include "skse_version.h"

#define PLUGIN_VERSION_MAJOR 1
#define PLUGIN_VERSION_MINOR 3
#define PLUGIN_VERSION_REVISION 5

#define PLUGIN_VERSION_VERSTRING STR(PLUGIN_VERSION_MAJOR) \
"." STR(PLUGIN_VERSION_MINOR) "." STR(PLUGIN_VERSION_REVISION)

#define MAKE_PLUGIN_VERSION(major, minor, rev) (((major & 0xFF) << 16) | ((minor & 0xFF) << 8) | (rev & 0xFF))

#define GET_PLUGIN_VERSION_MAJOR(a) (((a)&0x00FF0000) >> 16)
#define GET_PLUGIN_VERSION_MINOR(a) (((a)&0x0000FF00) >> 4)
#define GET_PLUGIN_VERSION_REV(a) (((a)&0x000000FF) >> 0)
