#pragma once

#ifdef ECHO_RESIDENT_TABLES
#define ECHO_RESIDENT __attribute__((section(".resident")))
#else
#define ECHO_RESIDENT
#endif

#ifdef ECHO_NP_FACES_SECTIONS
#define ECHO_NP_FACES_CODE __attribute__((section(".np_faces.text")))
#define ECHO_NP_FACES_DATA __attribute__((section(".np_faces.rodata")))
#define ECHO_NP_FACES_ENTRY __attribute__((section(".np_faces.entry"), used))
#else
#define ECHO_NP_FACES_CODE
#define ECHO_NP_FACES_DATA
#define ECHO_NP_FACES_ENTRY
#endif
