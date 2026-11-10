#pragma once

#include <stdbool.h>

typedef enum NpFacesState {
    NP_FACES_UNKNOWN,
    NP_FACES_READY,
    NP_FACES_FAILED
} NpFacesState;

typedef struct NpFacesGate {
    NpFacesState state;
    bool retried;
} NpFacesGate;

void np_faces_gate_forget(NpFacesGate *gate);
void np_faces_gate_checked(NpFacesGate *gate, bool header_intact);
bool np_faces_gate_should_load(NpFacesGate *gate, bool allowed);
void np_faces_gate_loaded(NpFacesGate *gate, bool ok);
