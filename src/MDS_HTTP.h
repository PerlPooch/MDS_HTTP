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

#pragma once

#include <Arduino.h>

#if defined(ARDUINO_ARCH_ESP8266)
#include <ESP8266WebServer.h>
using MDS_HTTPServer = ESP8266WebServer;
#else
#include <WebServer.h>
using MDS_HTTPServer = WebServer;
#endif

// Thin server owner; applications register routes directly through server().
class MDS_HTTP {
public:
	// Construct the owned WebServer on port (80 by default).
	explicit MDS_HTTP(uint16_t port = 80);
	MDS_HTTP(const MDS_HTTP&) = delete;
	MDS_HTTP& operator=(const MDS_HTTP&) = delete;

	// Start listening; repeated calls while running have no effect.
	void begin();

	// Dispatch requests while running.
	void tick();

	// Stop listening; route registrations are retained for the next begin().
	void stop();

	// Report whether begin() has been called without a subsequent stop().
	bool running() const { return running_; }

	// Borrow the owned server for route registration and request/response access.
	MDS_HTTPServer& server() { return server_; }
	const MDS_HTTPServer& server() const { return server_; }

private:
	MDS_HTTPServer server_;
	bool running_ = false;
};
