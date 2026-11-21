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

#pragma once

#include <Arduino.h>
#include <functional>
#include <memory>
#include <vector>

#if defined(ARDUINO_ARCH_ESP8266)
#include <ESP8266WebServer.h>
using MDS_HTTPServer = ESP8266WebServer;
#else
#include <WebServer.h>
using MDS_HTTPServer = WebServer;
#endif

class MDS_HTTPUpload;

// Thin server owner; applications register routes directly through server().
class MDS_HTTP {
public:
	enum class State {
		Stopped,
		WaitingForNetwork,
		Listening,
		Uploading,
		UploadComplete
	};

	enum class BrowserForm {
		Disabled,
		Enabled
	};

	enum class Error {
		None,
		InvalidUpload,
		UploadStartFailed,
		UploadWriteFailed,
		UploadFinalizeFailed,
		UploadAborted
	};

	// Report whether the network can accept HTTP requests.
	using ReadinessCallback = std::function<bool()>;

	// Observe committed lifecycle changes independently of diagnostic messages.
	using StateChangeCallback = std::function<void(State previous, State current)>;

	using StatusCallback = std::function<void(const String&)>;
	using UploadCompleteCallback = std::function<void()>;

	// Construct the owned WebServer on port (80 by default).
	explicit MDS_HTTP(uint16_t port = 80);

	// Cancel any active upload before releasing the server.
	~MDS_HTTP();

	// Prevent copying ownership of the web server.
	MDS_HTTP(const MDS_HTTP&) = delete;

	// Prevent assignment of an object that owns a web server.
	MDS_HTTP& operator=(const MDS_HTTP&) = delete;

	// Enable listening when the network callback is ready; repeated calls are safe.
	void begin();

	// Reconcile network readiness and dispatch requests while listening.
	void tick();

	// Disable listening until begin(), cancel uploads, and retain registered routes.
	void stop();

	// Return the lifecycle snapshot last reconciled by service or upload handling.
	State state() const {
		return state_;
	}

	// Return a stable diagnostic name for a lifecycle state.
	static const char* stateName(State state);

	// Report whether the listener is currently running, independently of enablement.
	bool running() const {
		return isListenerState(state_);
	}

	// Borrow the owned server for route registration and request/response access.
	MDS_HTTPServer& server() {
		return server_;
	}

	// Borrow a read-only view of the owned web server.
	const MDS_HTTPServer& server() const {
		return server_;
	}

	// Return the latest upload failure code; waiting and listener restarts preserve it.
	Error error() const {
		return error_;
	}

	// Return diagnostic text, or an empty string when error() is None.
	const char* lastError() const {
		return errorText_;
	}

	// Query connectivity during begin()/tick(); an unset callback means unavailable.
	// Keep the callback nonblocking; nullptr clears it for the next lifecycle check.
	void setNetworkReadyCallback(ReadinessCallback callback) {
		networkReadyCallback_ = callback;
	}

	// Register synchronous state changes; nullptr clears the optional callback.
	// Keep callbacks brief and do not re-enter the service.
	void setStateChangeCallback(StateChangeCallback callback) {
		stateChangeCallback_ = callback;
	}

	// Register listener and upload diagnostics; nullptr clears the callback and no output is printed directly.
	void setStatusCallback(StatusCallback callback) {
		statusCallback_ = callback;
	}

	// Register successful firmware upload notification after the response; nullptr clears it.
	void setUploadCompleteCallback(UploadCompleteCallback callback) {
		uploadCompleteCallback_ = callback;
	}

	// Register firmware POST and optionally a GET form at the same route before begin().
	void addFirmwareUpload(const char* route, BrowserForm form = BrowserForm::Disabled);

	// Register a unique multipart POST route before starting the server.
	void addUpload(const char* route, std::unique_ptr<MDS_HTTPUpload> handler);

private:
	friend class MDS_HTTPUpload;

	// Apply lifecycle changes and listener entry/exit actions once per transition.
	void transitionTo(State next);

	// Identify states in which the HTTP listener remains active.
	static bool isListenerState(State state);

	// Start or suspend the listener according to enablement and network readiness.
	void updateListener();

	// Deliver a listener or upload status message through the application callback.
	void status(const char* message);

	// Record an upload failure and notify the application.
	void fail(Error error, const char* message);

	// Clear the previous failure when a new upload starts or succeeds.
	void clearError();

	// Cancel unfinished requests when the listener stops or is destroyed.
	void cancelUploads();

	MDS_HTTPServer server_;
	State state_ = State::Stopped;
	ReadinessCallback networkReadyCallback_;
	Error error_ = Error::None;
	const char* errorText_ = "";
	StatusCallback statusCallback_;
	StateChangeCallback stateChangeCallback_;
	UploadCompleteCallback uploadCompleteCallback_;

	std::vector<std::function<void()>> uploadCancellations_;
};
