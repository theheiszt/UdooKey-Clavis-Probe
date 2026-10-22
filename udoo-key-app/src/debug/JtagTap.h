#pragma once

#include "JtagTypes.h"
#include <vector>

namespace udoo::debug {

/**
 * @brief IEEE 1149.1 TAP Controller State Machine.
 * Tracks current TAP state, calculates state transitions, and derives
 * minimal-length TMS sequences to navigate between any two TAP states.
 */
class JtagTap {
public:
    explicit JtagTap(JtagTapState initialState = JtagTapState::TestLogicReset);

    /// Get current TAP state
    JtagTapState currentState() const { return m_state; }

    /// Force internal state without clocking (e.g. after TRST or 5x TMS=1)
    void forceState(JtagTapState state) { m_state = state; }

    /// Advance state machine by one TCK pulse with given TMS
    JtagTapState clock(bool tms);

    /// Clock a sequence of TMS bits
    void clockSequence(const std::vector<bool>& tmsBits);

    /// Return the next state given a starting state and TMS value
    static JtagTapState nextState(JtagTapState from, bool tms);

    /// Return 5x TMS=1 bits to guarantee reset to TestLogicReset from ANY state
    static std::vector<bool> resetSequence();

    /// Calculate the optimal (shortest) TMS bit sequence to transition from 'from' to 'to'
    static std::vector<bool> shortestPath(JtagTapState from, JtagTapState to);

    /// Calculate the optimal TMS bit sequence to transition from current state to target state
    std::vector<bool> pathTo(JtagTapState targetState) const;

    /// Apply transition to target state, advancing internal state
    std::vector<bool> navigateTo(JtagTapState targetState);

private:
    JtagTapState m_state = JtagTapState::TestLogicReset;
};

} // namespace udoo::debug
