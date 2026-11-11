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

// Deliver a listener or upload status message through the application callback.
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

// Enable the service and immediately honor the current network prerequisite.
void MDS_HTTP::begin() {
	enabled_ = true;
	updateListener();
}

// Reconcile connectivity before dispatching requests on an active listener.
void MDS_HTTP::tick() {
	updateListener();

	if (running_) {
		server_.handleClient();
	}
}

// Disable automatic restarts until begin() is explicitly called again.
void MDS_HTTP::stop() {
	enabled_ = false;
	stopListener();
}

// Suspend on network loss without forgetting the application's intent to listen.
void MDS_HTTP::updateListener() {
	const bool networkReady = enabled_ && networkReadyCallback_ && networkReadyCallback_();

	if (!networkReady) {
		if (running_) {
			stopListener();
		}

		return;
	}

	if (!running_) {
		server_.begin();
		running_ = true;
		status("HTTP: listening");
	}
}

// Release upload resources and close the listener while preserving its routes.
void MDS_HTTP::stopListener() {
	if (cancelUpload_) {
		cancelUpload_();
	}

	if (running_) {
		server_.stop();
		running_ = false;
		status("HTTP: stopped");
	}
}
