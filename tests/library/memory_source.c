#include "memory_source.h"

#include <string.h>

static bool read_memory(void *context, uint32_t offset, void *buffer, uint32_t length)
{
    MemorySource *memory = context;
    memory->reads++;
    if (memory->failing || offset > memory->size || length > memory->size - offset) {
        return false;
    }
    memcpy(buffer, memory->bytes + offset, length);
    return true;
}

ByteSource memory_source(MemorySource *memory)
{
    return (ByteSource){.read = read_memory, .context = memory};
}
