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

#include "MDS_HTTP.h"

MDS_HTTP::MDS_HTTP(uint16_t port) : server_(port) {}

void MDS_HTTP::begin() {
	if (!running_) {
		server_.begin();
		running_ = true;
	}
}

void MDS_HTTP::tick() {
	if (running_) {
		server_.handleClient();
	}
}

void MDS_HTTP::stop() {
	if (running_) {
		server_.stop();
		running_ = false;
	}
}
