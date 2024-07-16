#pragma once

#include <util/base.h>

#define FF_LOG(level, format, ...) \
    blog(level, "[Streamlink Source]: " format, ##__VA_ARGS__)

#define FF_LOG_S(source, level, format, ...) \
    blog(level, "[Streamlink Source '%s']: " format, obs_source_get_name(source), ##__VA_ARGS__)

#define FF_BLOG(level, format, ...) \
    FF_LOG_S(s->source, level, format, ##__VA_ARGS__)
