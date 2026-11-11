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

// Verify that ordinary clients need neither upload routes nor updater linkage.
int main() {
	MDS_HTTP http;
	auto& server = http.server();
	assert(server.handlers.empty());
	assert(http.error() == MDS_HTTP::Error::None);
	assert(String(http.lastError()).empty());
	http.tick();
	assert(server.ticks == 0);
	http.setNetworkReadyCallback([]() {
		// Supply connectivity for the explicit lifecycle scenario.
		return true;
	});
	http.begin();
	http.begin();
	assert(server.starts == 1 && http.running());
	http.tick();
	assert(server.ticks == 1);
	http.stop();
	http.stop();
	assert(server.stops == 1 && !http.running());
	http.begin();
	assert(server.starts == 2);
	puts("PASS: basic HTTP lifecycle with no upload routes or updater dependency.");
}
