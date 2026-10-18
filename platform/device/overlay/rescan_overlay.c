#include "library/library_scan.h"

const LibraryScanEntry *rescan_overlay_entry(void);

const LibraryScanEntry *rescan_overlay_entry(void)
{
    return &LIBRARY_SCAN_ENTRY;
}
