#include "JtagTap.h"
#include <queue>
#include <array>
#include <algorithm>

namespace udoo::debug {

JtagTap::JtagTap(JtagTapState initialState)
    : m_state(initialState) {
}

JtagTapState JtagTap::nextState(JtagTapState from, bool tms) {
    switch (from) {
        case JtagTapState::TestLogicReset:
            return tms ? JtagTapState::TestLogicReset : JtagTapState::RunTestIdle;

        case JtagTapState::RunTestIdle:
            return tms ? JtagTapState::SelectDrScan : JtagTapState::RunTestIdle;

        case JtagTapState::SelectDrScan:
            return tms ? JtagTapState::SelectIrScan : JtagTapState::CaptureDr;

        case JtagTapState::CaptureDr:
            return tms ? JtagTapState::Exit1Dr : JtagTapState::ShiftDr;

        case JtagTapState::ShiftDr:
            return tms ? JtagTapState::Exit1Dr : JtagTapState::ShiftDr;

        case JtagTapState::Exit1Dr:
            return tms ? JtagTapState::UpdateDr : JtagTapState::PauseDr;

        case JtagTapState::PauseDr:
            return tms ? JtagTapState::Exit2Dr : JtagTapState::PauseDr;

        case JtagTapState::Exit2Dr:
            return tms ? JtagTapState::UpdateDr : JtagTapState::ShiftDr;

        case JtagTapState::UpdateDr:
            return tms ? JtagTapState::SelectDrScan : JtagTapState::RunTestIdle;

        case JtagTapState::SelectIrScan:
            return tms ? JtagTapState::TestLogicReset : JtagTapState::CaptureIr;

        case JtagTapState::CaptureIr:
            return tms ? JtagTapState::Exit1Ir : JtagTapState::ShiftIr;

        case JtagTapState::ShiftIr:
            return tms ? JtagTapState::Exit1Ir : JtagTapState::ShiftIr;

        case JtagTapState::Exit1Ir:
            return tms ? JtagTapState::UpdateIr : JtagTapState::PauseIr;

        case JtagTapState::PauseIr:
            return tms ? JtagTapState::Exit2Ir : JtagTapState::PauseIr;

        case JtagTapState::Exit2Ir:
            return tms ? JtagTapState::UpdateIr : JtagTapState::ShiftIr;

        case JtagTapState::UpdateIr:
            return tms ? JtagTapState::SelectDrScan : JtagTapState::RunTestIdle;

        default:
            return JtagTapState::TestLogicReset;
    }
}

JtagTapState JtagTap::clock(bool tms) {
    m_state = nextState(m_state, tms);
    return m_state;
}

void JtagTap::clockSequence(const std::vector<bool>& tmsBits) {
    for (bool tms : tmsBits) {
        clock(tms);
    }
}

std::vector<bool> JtagTap::resetSequence() {
    return { true, true, true, true, true };
}

std::vector<bool> JtagTap::shortestPath(JtagTapState from, JtagTapState to) {
    if (from == to) {
        return {};
    }

    // BFS on the 16-node state graph
    struct Edge {
        uint8_t parent = 255;
        bool tms = false;
        bool visited = false;
    };

    std::array<Edge, 16> graph;
    std::queue<uint8_t> q;

    uint8_t startIdx = static_cast<uint8_t>(from);
    uint8_t targetIdx = static_cast<uint8_t>(to);

    graph[startIdx].visited = true;
    q.push(startIdx);

    bool found = false;
    while (!q.empty()) {
        uint8_t curr = q.front();
        q.pop();

        if (curr == targetIdx) {
            found = true;
            break;
        }

        // Try TMS = 0, then TMS = 1
        for (int tmsVal = 0; tmsVal <= 1; ++tmsVal) {
            bool tms = (tmsVal != 0);
            JtagTapState nxt = nextState(static_cast<JtagTapState>(curr), tms);
            uint8_t nxtIdx = static_cast<uint8_t>(nxt);

            if (!graph[nxtIdx].visited) {
                graph[nxtIdx].visited = true;
                graph[nxtIdx].parent = curr;
                graph[nxtIdx].tms = tms;
                q.push(nxtIdx);
            }
        }
    }

    if (!found) {
        // Fallback: 5x TMS=1 to reset
        return resetSequence();
    }

    // Backtrack from target to start
    std::vector<bool> path;
    uint8_t trace = targetIdx;
    while (trace != startIdx) {
        path.push_back(graph[trace].tms);
        trace = graph[trace].parent;
    }

    std::reverse(path.begin(), path.end());
    return path;
}

std::vector<bool> JtagTap::pathTo(JtagTapState targetState) const {
    return shortestPath(m_state, targetState);
}

std::vector<bool> JtagTap::navigateTo(JtagTapState targetState) {
    std::vector<bool> seq = shortestPath(m_state, targetState);
    clockSequence(seq);
    return seq;
}

} // namespace udoo::debug
