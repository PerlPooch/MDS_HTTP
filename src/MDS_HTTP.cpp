/*
 * ===========================================================================
 * MDS_HTTP 0.4.0 - https://home.marcspencer.com/mds-http
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

// Translate lifecycle states into stable diagnostic names.
const char* MDS_HTTP::stateName(State state) {
	switch (state) {
		case State::Stopped:
			return "Stopped";
		case State::WaitingForNetwork:
			return "WaitingForNetwork";
		case State::Listening:
			return "Listening";
		case State::Uploading:
			return "Uploading";
		case State::UploadComplete:
			return "UploadComplete";
	}

	return "Unknown";
}

// Upload progress and completion retain the same active listener.
bool MDS_HTTP::isListenerState(State state) {
	return state == State::Listening || state == State::Uploading || state == State::UploadComplete;
}

// Centralize listener actions and notify applications after each committed state change.
void MDS_HTTP::transitionTo(State next) {
	if (state_ == next) {
		return;
	}

	const bool nextHasListener = isListenerState(next);
	if (running() && !nextHasListener) {
		if (cancelUpload_) {
			// Cancellation reports upload completion before the listener leaves service.
			cancelUpload_();
		}

		server_.stop();
	} else if (!running() && nextHasListener) {
		server_.begin();
	}

	const State previous = state_;
	state_ = next;

	if (stateChangeCallback_) {
		stateChangeCallback_(previous, next);
	}

	const String message = String("HTTP: ") + stateName(previous) + " -> " + stateName(next);
	status(message.c_str());
}

// Enable the service and immediately honor the current network prerequisite.
void MDS_HTTP::begin() {
	updateListener();
}

// Reconcile connectivity before dispatching requests on an active listener.
void MDS_HTTP::tick() {
	if (state_ == State::Stopped) {
		return;
	}

	updateListener();

	if (running()) {
		server_.handleClient();
	}
}

// Disable automatic restarts until begin() is explicitly called again.
void MDS_HTTP::stop() {
	transitionTo(State::Stopped);
}

// Preserve upload states while connected and suspend listening when readiness is lost.
void MDS_HTTP::updateListener() {
	const bool networkReady = networkReadyCallback_ && networkReadyCallback_();

	if (!networkReady) {
		transitionTo(State::WaitingForNetwork);
	} else if (!running()) {
		transitionTo(State::Listening);
	}
}
