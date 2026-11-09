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

// Construct the owned web server on the requested port without starting it.
MDS_HTTP::MDS_HTTP(uint16_t port) : server_(port) {
}

// Cancel any active firmware write before releasing the server.
MDS_HTTP::~MDS_HTTP() {
	if (cancelUpload_) {
		cancelUpload_();
	}
}

// Deliver an upload status message through the application callback.
void MDS_HTTP::status(const char* message) {
	if (statusCallback_) {
		statusCallback_(String(message));
	}
}

// Record an upload failure and notify the application.
void MDS_HTTP::fail(Error error, const char* message) {
	error_ = error;
	errorText_ = message;
	status(message);
}

// Clear the previous failure when a new upload starts or succeeds.
void MDS_HTTP::clearError() {
	error_ = Error::None;
	errorText_ = "";
}

// Start listening unless this server is already running.
void MDS_HTTP::begin() {
	if (!running_) {
		server_.begin();
		running_ = true;
	}
}

// Dispatch pending HTTP requests only while the server is running.
void MDS_HTTP::tick() {
	if (running_) {
		server_.handleClient();
	}
}

// Stop listening while keeping routes registered for a later begin().
void MDS_HTTP::stop() {
	if (cancelUpload_) {
		cancelUpload_();
	}

	if (running_) {
		server_.stop();
		running_ = false;
	}
}
