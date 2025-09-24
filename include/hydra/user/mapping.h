#pragma once

#include <stddef.h>

typedef struct _mapping_info {
    void* base = nullptr;
    size_t size = 0;
    size_t header_size = 0;
} mapping_info;
