#include "library/library_scan.h"

const LibraryScanEntry LIBRARY_SCAN_ENTRY = {
    .magic = LIBRARY_SCAN_MAGIC,
    .scan = library_scan,
};
