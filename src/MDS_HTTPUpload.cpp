/*
 * ===========================================================================
 * MDS_HTTP 0.2.0 - https://home.marcspencer.com/mds-http
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

#if defined(ARDUINO_ARCH_ESP8266)
#include <Updater.h>
#else
#include <Update.h>
#endif

// Install upload routes only for applications that construct this optional feature.
MDS_HTTP::MDS_HTTP(const Options& options, uint16_t port) : MDS_HTTP(port) {
	if (options.firmwareUpload == UploadMode::Enabled) {
		server_.on("/upload", HTTP_GET, [this]() {
			// Serve the common firmware selection form.
			handleUploadPage();
		});
		server_.on(
			"/upload", HTTP_POST,
			[this]() {
				// Finish the response before the application schedules its reboot.
				handleUploadPost();
			},
			[this]() {
				// Process each multipart firmware chunk synchronously.
				handleUploadStream();
			});
		cancelUpload_ = [this]() {
			// Cancel a partial upload when the listener stops or is destroyed.
			if (uploadActive_) {
				abortUpload();
				fail(Error::UploadAborted, "OTA: Upload aborted");
			}

			uploadSeen_ = false;
			uploadComplete_ = false;
		};
	}
}

// Serve the existing browser form for uploading a firmware binary.
void MDS_HTTP::handleUploadPage() {
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
		"<form method='POST' action='/upload' enctype='multipart/form-data'>"
		"<div class='row'>"
		"<input type='file' name='firmware' accept='.bin' required>"
		"<button type='submit'>Update</button>"
		"</div>"
		"</form>"
		"</div>"
		"</body></html>";

	server_.send(200, "text/html", uploadHtml);
}

// Respond only after a complete firmware image has been accepted by the updater.
void MDS_HTTP::handleUploadPost() {
	bool success = uploadComplete_ && uploadActive_ && error_ == Error::None;

	// Commit only after the complete multipart request has been accepted.
	if (success) {
		if (Update.end(true)) {
			uploadActive_ = false;
			clearError();
			status("OTA: Success.");
		} else {
			abortUpload();
			fail(Error::UploadFinalizeFailed, "OTA: Update.end() failed");
			success = false;
		}
	}

	if (!success && error_ == Error::None) {
		abortUpload();
		fail(Error::InvalidUpload, "OTA: Missing or incomplete firmware upload");
	}

	uploadSeen_ = false;
	uploadComplete_ = false;

	if (!success) {
		server_.send(500, "text/plain", lastError());
		return;
	}

	server_.send(200, "text/plain", "Update OK. Rebooting...\n");

	if (uploadCompleteCallback_) {
		uploadCompleteCallback_();
	}
}

// Abort unfinished writes without validating or activating the partial image.
void MDS_HTTP::abortUpload() {
	if (uploadActive_) {
#if defined(ARDUINO_ARCH_ESP8266)
		// ESP8266 releases an unfinished update through end(false); it has no abort() API.
		Update.end(false);
#else
		Update.abort();
#endif
		uploadActive_ = false;
	}

	uploadComplete_ = false;
}

// Handle multipart events without continuing a failed firmware write.
void MDS_HTTP::handleUploadStream() {
	HTTPUpload& upload = server_.upload();

	switch (upload.status) {
		case UPLOAD_FILE_START:
			if (uploadSeen_) {
				abortUpload();
				fail(Error::InvalidUpload, "OTA: Expected one firmware file");
				return;
			}

			uploadSeen_ = true;
			uploadComplete_ = false;
			clearError();
			status("OTA: Start ...");

			if (upload.name != "firmware") {
				fail(Error::InvalidUpload, "OTA: Expected firmware field");
				return;
			}

#if defined(ARDUINO_ARCH_ESP8266)
			// ESP8266 requires the available sketch space rather than an unknown length.
			if (!Update.begin((ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000)) {
#else
			if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
#endif
				fail(Error::UploadStartFailed, "OTA: Update.begin() failed");
				return;
			}

			uploadActive_ = true;
			break;

		case UPLOAD_FILE_WRITE:
			if (!uploadActive_) {
				return;
			}

			if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
				abortUpload();
				fail(Error::UploadWriteFailed, "OTA: Update.write() failed");
			}
			break;

		case UPLOAD_FILE_END:
			if (!uploadActive_) {
				return;
			}

			if (upload.totalSize == 0) {
				abortUpload();
				fail(Error::InvalidUpload, "OTA: Empty firmware upload");
				return;
			}

			// The POST handler validates and activates the image after all form parts arrive.
			uploadComplete_ = true;
			break;

		case UPLOAD_FILE_ABORTED:
			abortUpload();
			uploadSeen_ = false;
			fail(Error::UploadAborted, "OTA: Upload aborted");
			break;
	}
}
