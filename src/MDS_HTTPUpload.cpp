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

#include "MDS_HTTPUpload.h"

#include <utility>

#if defined(ARDUINO_ARCH_ESP8266)
#include <Updater.h>
#else
#include <Update.h>
#endif

namespace {

// Back the OTA route with the platform updater and its existing diagnostics.
class FirmwareUpload final : public MDS_HTTPUpload {
public:
	// Keep the firmware field name and messages used by existing OTA clients.
	explicit FirmwareUpload(const MDS_HTTP::UploadCompleteCallback& completeCallback)
		: MDS_HTTPUpload("firmware", firmwareMessages()), completeCallback_(completeCallback) {
	}

private:
	// Reserve space in the inactive firmware slot.
	bool begin(const HTTPUpload&) override {
#if defined(ARDUINO_ARCH_ESP8266)
		// ESP8266 requires the available sketch space rather than an unknown length.
		return Update.begin((ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000);
#else
		return Update.begin(UPDATE_SIZE_UNKNOWN);
#endif
	}

	// Treat a short write as a failed chunk.
	bool write(uint8_t* data, size_t size) override {
		return Update.write(data, size) == size;
	}

	// Validate and activate the image using the bytes received.
	bool commit() override {
		return Update.end(true);
	}

	// Release the updater without activating an unfinished image.
	void abort() override {
#if defined(ARDUINO_ARCH_ESP8266)
		// ESP8266 has no abort() API.
		Update.end(false);
#else
		Update.abort();
#endif
	}

	// Schedule application work only once the HTTP response has been sent.
	void onSuccess(MDS_HTTP&) override {
		if (completeCallback_) {
			completeCallback_();
		}
	}

	// Preserve the existing OTA diagnostics and response body.
	static Messages firmwareMessages() {
		Messages messages;
		messages.started = "OTA: Start ...";
		messages.succeeded = "OTA: Success.";
		messages.successResponse = "Update OK. Rebooting...\n";
		messages.incomplete = "OTA: Missing or incomplete firmware upload";
		messages.multiple = "OTA: Expected one firmware file";
		messages.wrongField = "OTA: Expected firmware field";
		messages.empty = "OTA: Empty firmware upload";
		messages.startFailed = "OTA: Update.begin() failed";
		messages.writeFailed = "OTA: Update.write() failed";
		messages.commitFailed = "OTA: Update.end() failed";
		messages.aborted = "OTA: Upload aborted";
		return messages;
	}

	const MDS_HTTP::UploadCompleteCallback& completeCallback_;
};

// Serve the existing browser form for uploading a firmware binary.
void sendFirmwarePage(MDS_HTTPServer& server) {
	static const char uploadHtml[] =
		"<!doctype html><html><head><meta charset='utf-8'>"
		"<meta name='viewport' content='width=device-width,initial-scale=1'>"
		"<title>Update Firmware</title>"
		"<style>"
		":root{--bg:#000000;--text:#fff;--card:#20202080;--border:rgba(43,14,161,0.58);--shadow:0 12px 30px rgba(0,0,0,.10);--radius:8px}"
		"html,body{height:100%;}"
		"body{margin:0;color:var(--text);background:var(--bg);font-family:Helvetica,Arial,sans-serif;}"
		".card{max-width:560px;margin:10vh auto;padding:24px;background:var(--card);border:1px solid var(--border);border-radius:var(--radius);box-shadow:var(--shadow);}"
		".row{display:flex;gap:8px;align-items:center;}"
		".row button{margin-left:auto;}"
		"h3{margin:0 0 14px 0;font-size:18px;font-weight:600;}"
		"input,button{font:inherit;}"
		"button{margin-left:8px;border-radius:var(--radius);background:#fff;cursor:pointer;}"
		"</style>"
		"</head><body>"
		"<div class='card'>"
		"<h3>Update Firmware</h3>"
		"<form method='POST' enctype='multipart/form-data'>"
		"<div class='row'>"
		"<input type='file' name='firmware' accept='.bin' required>"
		"<button type='submit'>Update</button>"
		"</div>"
		"</form>"
		"</div>"
		"</body></html>";

	server.send(200, "text/html", uploadHtml);
}

} // namespace

// Register firmware upload at the application route and optionally serve its form.
void MDS_HTTP::addFirmwareUpload(const char* route, BrowserForm form) {
	if (form == BrowserForm::Enabled) {
		server_.on(route, HTTP_GET, [this]() {
			// Serve a form that posts back to the current URL.
			sendFirmwarePage(server_);
		});
	}

	addUpload(route, std::unique_ptr<MDS_HTTPUpload>(new FirmwareUpload(uploadCompleteCallback_)));
}

// Install multipart callbacks and retain ownership for the lifetime of the route.
void MDS_HTTP::addUpload(const char* route, std::unique_ptr<MDS_HTTPUpload> handler) {
	if (!handler) {
		return;
	}

	std::shared_ptr<MDS_HTTPUpload> ownedUpload(std::move(handler));
	auto* upload = ownedUpload.get();
	uploadCancellations_.push_back([this, ownedUpload]() {
		// Retain the handler for the lifetime of the registered callbacks.
		ownedUpload->cancel(*this);
	});
	server_.on(route, HTTP_POST, [this, upload]() {
		// Commit only after the server has processed the complete request.
		upload->handlePost(*this);
	}, [this, upload]() {
		// Forward each multipart event to this route's handler.
		upload->handleStream(*this);
	});
}

// Commit the upload and send the response before notifying a successful backend.
void MDS_HTTPUpload::handlePost(MDS_HTTP& http) {
	using Error = MDS_HTTP::Error;

	if (complete_ && active_ && error_ == Error::None) {
		if (commit()) {
			active_ = false;
			http.clearError();
			http.status(messages_.succeeded);
		} else {
			abortWrite();
			fail(http, MDS_HTTP::Error::UploadFinalizeFailed, messages_.commitFailed);
		}
	} else if (error_ == Error::None) {
		abortWrite();
		fail(http, MDS_HTTP::Error::InvalidUpload, messages_.incomplete);
	}

	seen_ = false;
	complete_ = false;

	if (error_ != Error::None) {
		// Restore this route's result if another route has reported an error since.
		http.error_ = error_;
		http.errorText_ = errorText_;
		sendResponse(http.server_, error_, errorText_);
		error_ = Error::None;
		errorText_ = "";
		http.transitionTo(MDS_HTTP::State::UploadComplete);
		return;
	}

	sendResponse(http.server_, Error::None, messages_.successResponse);
	http.transitionTo(MDS_HTTP::State::UploadComplete);
	onSuccess(http);
}

// Preserve the original response contract for ordinary upload handlers.
void MDS_HTTPUpload::sendResponse(MDS_HTTPServer& server, MDS_HTTP::Error error, const char* message) {
	server.send(error == MDS_HTTP::Error::None ? 200 : 500, "text/plain", message);
}

// Route each multipart event to the backend while keeping each request's state separate.
void MDS_HTTPUpload::handleStream(MDS_HTTP& http) {
	HTTPUpload& upload = http.server_.upload();
	using Error = MDS_HTTP::Error;
	using State = MDS_HTTP::State;

	switch (upload.status) {
		case UPLOAD_FILE_START:
			if (seen_) {
				abortWrite();
				fail(http, Error::InvalidUpload, messages_.multiple);
				http.transitionTo(State::UploadComplete);
				return;
			}

			seen_ = true;
			complete_ = false;
			error_ = Error::None;
			errorText_ = "";
			http.clearError();
			http.transitionTo(State::Uploading);
			http.status(messages_.started);

			if (upload.name != field_) {
				fail(http, Error::InvalidUpload, messages_.wrongField);
				http.transitionTo(State::UploadComplete);
				return;
			}

			if (!begin(upload)) {
				fail(http, Error::UploadStartFailed, messages_.startFailed);
				http.transitionTo(State::UploadComplete);
				return;
			}

			active_ = true;
			break;

		case UPLOAD_FILE_WRITE:
			if (active_ && !write(upload.buf, upload.currentSize)) {
				abortWrite();
				fail(http, Error::UploadWriteFailed, messages_.writeFailed);
				http.transitionTo(State::UploadComplete);
			}
			break;

		case UPLOAD_FILE_END:
			if (!active_) {
				return;
			}

			if (!allowEmpty_ && upload.totalSize == 0) {
				abortWrite();
				fail(http, Error::InvalidUpload, messages_.empty);
				http.transitionTo(State::UploadComplete);
				return;
			}

			// Wait for the POST callback in case another multipart part follows.
			complete_ = true;
			break;

		case UPLOAD_FILE_ABORTED:
			abortWrite();
			seen_ = false;
			fail(http, Error::UploadAborted, messages_.aborted);
			http.transitionTo(State::UploadComplete);
			break;
	}
}

// Clear multipart state and report cancellation only when storage was active.
void MDS_HTTPUpload::cancel(MDS_HTTP& http) {
	if (active_) {
		abortWrite();
		fail(http, MDS_HTTP::Error::UploadAborted, messages_.aborted);
		http.transitionTo(MDS_HTTP::State::UploadComplete);
	}

	seen_ = false;
	complete_ = false;
}

// Abort storage only once and discard any pending commit.
void MDS_HTTPUpload::abortWrite() {
	if (active_) {
		abort();
		active_ = false;
	}

	complete_ = false;
}

// Keep each route's response separate while updating the public diagnostics.
void MDS_HTTPUpload::fail(MDS_HTTP& http, MDS_HTTP::Error error, const char* message) {
	error_ = error;
	errorText_ = message;
	http.fail(error, message);
}
