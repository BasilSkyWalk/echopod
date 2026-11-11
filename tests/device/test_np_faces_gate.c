#include "check.h"
#include "device/np_faces_gate.h"

static NpFacesGate ready_gate(void)
{
    NpFacesGate gate = {0};
    CHECK(np_faces_gate_should_load(&gate, true));
    np_faces_gate_loaded(&gate, true);
    return gate;
}

static void a_new_gate_loads_once_and_then_is_ready(void)
{
    NpFacesGate gate = {0};
    CHECK(np_faces_gate_should_load(&gate, true));
    np_faces_gate_loaded(&gate, true);
    CHECK(gate.state == NP_FACES_READY);
    CHECK(!np_faces_gate_should_load(&gate, true));
}

static void a_failed_load_stays_failed_and_never_asks_again(void)
{
    NpFacesGate gate = {0};
    CHECK(np_faces_gate_should_load(&gate, true));
    np_faces_gate_loaded(&gate, false);
    CHECK(gate.state == NP_FACES_FAILED);
    CHECK(!np_faces_gate_should_load(&gate, true));
    np_faces_gate_forget(&gate);
    CHECK(gate.state == NP_FACES_FAILED);
}

static void a_load_that_is_not_allowed_fails_without_loading(void)
{
    NpFacesGate gate = {0};
    CHECK(!np_faces_gate_should_load(&gate, false));
    CHECK(gate.state == NP_FACES_FAILED);
    CHECK(!np_faces_gate_should_load(&gate, true));
}

static void an_intact_header_keeps_the_gate_ready_on_every_call(void)
{
    NpFacesGate gate = ready_gate();
    for (int call = 0; call < 5; call++) {
        np_faces_gate_checked(&gate, true);
        CHECK(gate.state == NP_FACES_READY);
    }
    CHECK(!gate.retried);
}

static void a_bad_header_allows_one_reload_and_the_second_fails_for_good(void)
{
    NpFacesGate gate = ready_gate();
    np_faces_gate_checked(&gate, false);
    CHECK(gate.state == NP_FACES_UNKNOWN);
    CHECK(np_faces_gate_should_load(&gate, true));
    np_faces_gate_loaded(&gate, true);
    np_faces_gate_checked(&gate, false);
    CHECK(gate.state == NP_FACES_FAILED);
    CHECK(!np_faces_gate_should_load(&gate, true));
}

static void forgetting_sends_a_ready_gate_back_to_unknown_without_using_the_retry(void)
{
    NpFacesGate gate = ready_gate();
    np_faces_gate_forget(&gate);
    CHECK(gate.state == NP_FACES_UNKNOWN);
    CHECK(!gate.retried);
    CHECK(np_faces_gate_should_load(&gate, true));
}

static void a_header_check_on_a_gate_that_is_not_ready_changes_nothing(void)
{
    NpFacesGate gate = {0};
    np_faces_gate_checked(&gate, false);
    CHECK(gate.state == NP_FACES_UNKNOWN);
    CHECK(!gate.retried);
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(a_new_gate_loads_once_and_then_is_ready),
        TEST_CASE(a_failed_load_stays_failed_and_never_asks_again),
        TEST_CASE(a_load_that_is_not_allowed_fails_without_loading),
        TEST_CASE(an_intact_header_keeps_the_gate_ready_on_every_call),
        TEST_CASE(a_bad_header_allows_one_reload_and_the_second_fails_for_good),
        TEST_CASE(forgetting_sends_a_ready_gate_back_to_unknown_without_using_the_retry),
        TEST_CASE(a_header_check_on_a_gate_that_is_not_ready_changes_nothing),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}
