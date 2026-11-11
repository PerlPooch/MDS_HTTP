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
#include <functional>

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
	enum class UploadMode {
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

	struct Options {
		UploadMode firmwareUpload = UploadMode::Disabled;
	};

	// Report whether the network can accept HTTP requests.
	using ReadinessCallback = std::function<bool()>;

	using StatusCallback = std::function<void(const String&)>;
	using UploadCompleteCallback = std::function<void()>;

	// Construct the owned WebServer on port (80 by default).
	explicit MDS_HTTP(uint16_t port = 80);

	// Register optional firmware upload routes before the server starts listening.
	explicit MDS_HTTP(const Options& options, uint16_t port = 80);

	// Cancel any active firmware write before releasing the server.
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

	// Report whether the listener is currently running, independently of enablement.
	bool running() const {
		return running_;
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

	// Register listener and upload diagnostics; nullptr clears the callback and no output is printed directly.
	void setStatusCallback(StatusCallback callback) {
		statusCallback_ = callback;
	}

	// Register successful upload notification after the HTTP response is sent; nullptr clears it.
	void setUploadCompleteCallback(UploadCompleteCallback callback) {
		uploadCompleteCallback_ = callback;
	}

private:
	// Start or suspend the listener according to enablement and network readiness.
	void updateListener();

	// Cancel any upload and close the listener without changing enablement.
	void stopListener();

	// Deliver a listener or upload status message through the application callback.
	void status(const char* message);

	// Record an upload failure and notify the application.
	void fail(Error error, const char* message);

	// Clear the previous failure when a new upload starts or succeeds.
	void clearError();

	// Serve the browser firmware selection form.
	void handleUploadPage();

	// Respond to the completed POST and notify the application only on success.
	void handleUploadPost();

	// Write firmware chunks and mark their completion, aborting failed transfers.
	void handleUploadStream();

	// Abort the platform updater and leave the active write state.
	void abortUpload();

	MDS_HTTPServer server_;
	bool enabled_ = false;
	bool running_ = false;
	ReadinessCallback networkReadyCallback_;
	Error error_ = Error::None;
	const char* errorText_ = "";
	StatusCallback statusCallback_;
	UploadCompleteCallback uploadCompleteCallback_;

	// Installed only by the upload constructor, keeping updater code out of basic clients.
	std::function<void()> cancelUpload_;
	bool uploadSeen_ = false;
	bool uploadActive_ = false;
	bool uploadComplete_ = false;
};
