#include "device/np_faces_gate.h"

void np_faces_gate_forget(NpFacesGate *gate)
{
    if (gate->state == NP_FACES_READY) {
        gate->state = NP_FACES_UNKNOWN;
    }
}

void np_faces_gate_checked(NpFacesGate *gate, bool header_intact)
{
    if (gate->state != NP_FACES_READY || header_intact) {
        return;
    }
    gate->state = gate->retried ? NP_FACES_FAILED : NP_FACES_UNKNOWN;
    gate->retried = true;
}

bool np_faces_gate_should_load(NpFacesGate *gate, bool allowed)
{
    if (gate->state != NP_FACES_UNKNOWN) {
        return false;
    }
    if (!allowed) {
        gate->state = NP_FACES_FAILED;
    }
    return allowed;
}

void np_faces_gate_loaded(NpFacesGate *gate, bool ok)
{
    gate->state = ok ? NP_FACES_READY : NP_FACES_FAILED;
}
