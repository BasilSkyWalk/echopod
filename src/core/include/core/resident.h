#pragma once

#ifdef ECHO_RESIDENT_TABLES
#define ECHO_RESIDENT __attribute__((section(".resident")))
#else
#define ECHO_RESIDENT
#endif
