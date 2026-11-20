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

#include "MDS_HTTP.h"

// Process one multipart file per POST using storage supplied by a subclass.
class MDS_HTTPUpload {
public:
	// Permit MDS_HTTP to destroy a registered backend through its base pointer.
	virtual ~MDS_HTTPUpload() = default;

protected:
	// Customize stable diagnostics and the response body for a storage backend.
	struct Messages {
		const char* started = "Upload: Start";
		const char* succeeded = "Upload: Success";
		const char* successResponse = "Upload OK.\n";
		const char* incomplete = "Upload: Missing or incomplete file";
		const char* multiple = "Upload: Expected one file";
		const char* wrongField = "Upload: Unexpected file field";
		const char* empty = "Upload: Empty file";
		const char* startFailed = "Upload: Could not start";
		const char* writeFailed = "Upload: Could not write";
		const char* commitFailed = "Upload: Could not commit";
		const char* aborted = "Upload: Aborted";
	};

	// Keep the expected form field and message strings for the handler's lifetime.
	explicit MDS_HTTPUpload(const char* field, bool allowEmpty = false)
		: MDS_HTTPUpload(field, Messages{}, allowEmpty) {
	}

	// Configure a backend with its own messaging and empty-file policy.
	MDS_HTTPUpload(const char* field, const Messages& messages, bool allowEmpty = false)
		: field_(field), messages_(messages), allowEmpty_(allowEmpty) {
	}

	// Start receiving a file. A failed start must leave no pending write.
	virtual bool begin(const HTTPUpload& upload) = 0;

	// Accept the entire chunk or return false to abort the request.
	virtual bool write(uint8_t* data, size_t size) = 0;

	// Publish the completed upload after all multipart fields have arrived.
	virtual bool commit() = 0;

	// Release any pending write, including after a failed commit.
	virtual void abort() = 0;

	// Notify the backend after a successful HTTP response and state transition.
	virtual void onSuccess(MDS_HTTP&) {
	}

private:
	friend class MDS_HTTP;

	// Respond to the completed request, committing only if the full file arrived.
	void handlePost(MDS_HTTP& http);

	// Forward multipart events to the backend while tracking request state.
	void handleStream(MDS_HTTP& http);

	// Abort a partial transfer and reset its multipart state on listener shutdown.
	void cancel(MDS_HTTP& http);

	// Abort storage only when it holds a pending write.
	void abortWrite();

	// Record the route's error and publish it through the server diagnostics.
	void fail(MDS_HTTP& http, MDS_HTTP::Error error, const char* message);

	const char* field_;
	Messages messages_;
	bool allowEmpty_;
	MDS_HTTP::Error error_ = MDS_HTTP::Error::None;
	const char* errorText_ = "";
	bool seen_ = false;
	bool active_ = false;
	bool complete_ = false;
};
