/*
 * ===========================================================================
 * MDS_HTTP 0.1.0 - https://home.marcspencer.com/mds-http
 *
 * Copyright (c) 2026 Marc D. Spencer.
 * All Rights Reserved under the applicable license.
 * SPDX-License-Identifier: MPL-2.0
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 * ===========================================================================
 */

#include <MDS_HTTP.h>
#include <cassert>
#include <cstdio>
#include <vector>

// Verify readiness suspension is distinct from an explicit application shutdown.
int main() {
    MDS_HTTP http;
    auto& server = http.server();
    bool networkReady = false;
    int readinessQueries = 0;
    std::vector<String> messages;

    server.on("/", HTTP_GET, []() {
        // Retain an application route across listener suspension and restart.
    });
    http.setStatusCallback([&](const String& message) {
        // Verify transitions are reported once instead of on every tick.
        messages.push_back(message);
    });
    http.begin();
    http.tick();
    assert(!http.running() && server.starts == 0 && server.ticks == 0);
    assert(http.error() == MDS_HTTP::Error::None && messages.empty());

    http.setNetworkReadyCallback([&]() {
        // Count evaluations and reflect live connectivity changes.
        ++readinessQueries;
        return networkReady;
    });
    http.tick();
    assert(readinessQueries == 1 && !http.running());
    networkReady = true;
    http.tick();
    assert(readinessQueries == 2 && server.starts == 1 && server.ticks == 1);
    assert(http.running() && messages.size() == 1 && messages.back() == "HTTP: listening");
    http.tick();
    assert(readinessQueries == 3 && server.starts == 1 && server.ticks == 2);
    assert(messages.size() == 1);

    networkReady = false;
    http.tick();
    assert(!http.running() && server.stops == 1 && server.ticks == 2);
    assert(messages.size() == 2 && messages.back() == "HTTP: stopped");
    http.tick();
    assert(server.stops == 1 && messages.size() == 2);
    networkReady = true;
    http.tick();
    assert(http.running() && server.starts == 2 && server.handlers.size() == 1);

    http.setNetworkReadyCallback(nullptr);
    http.tick();
    assert(!http.running() && server.stops == 2);
    http.tick();
    assert(server.stops == 2 && http.error() == MDS_HTTP::Error::None);

    http.setNetworkReadyCallback([&]() {
        // A replacement callback may resume only an enabled service.
        ++readinessQueries;
        return networkReady;
    });
    http.tick();
    assert(http.running() && server.starts == 3);
    http.stop();
    http.stop();
    assert(!http.running() && server.stops == 3);

    const int queriesBeforeShutdownTicks = readinessQueries;
    http.tick();
    networkReady = false;
    http.tick();
    networkReady = true;
    http.tick();
    assert(readinessQueries == queriesBeforeShutdownTicks && server.starts == 3);
    assert(!http.running());

    http.begin();
    http.begin();
    assert(http.running() && server.starts == 4);
    assert(server.handlers.size() == 1);
    puts("PASS: HTTP missing/cleared callbacks, reconnect, transition diagnostics and persistent explicit shutdown.");
}
