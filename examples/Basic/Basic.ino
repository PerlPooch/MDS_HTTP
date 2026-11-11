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

#include <MDS_WiFi.h>
#include <MDS_HTTP.h>

MDS_WiFi wifi;
MDS_HTTP http;  // Port 80 by default.

// Register routes and connectivity before enabling HTTP.
void setup() {
    Serial.begin(115200);

    // Register application routes before starting the server.
    http.server().on("/", HTTP_GET, []() {
        http.server().send(200, "text/plain", "Hello from MDS_HTTP!\n");
    });

    MDS_WiFi::Options options;

    options.accessPointPrefix = "Web Demo";
    options.connectionPolicy =
        MDS_WiFi::ConnectionPolicy::RetrySavedNetworkAsync;

    wifi.begin(options);

    http.setNetworkReadyCallback([]() {
        // Let the connection manager define station and IP readiness.
        return wifi.ready();
    });
    http.setStatusCallback([](const String& message) {
        // Report listener transitions without managing its lifecycle here.
        Serial.println(message);
        if (http.running()) {
            Serial.print("Open http://");
            Serial.println(WiFi.localIP());
        }
    });
    http.begin();
}

// Service connectivity and HTTP while other application work continues.
void loop() {
    wifi.tick();
    http.tick();

    // Do other application work here.
}
