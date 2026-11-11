/*
 * ===========================================================================
 * MDS_HTTP - https://home.marcspencer.com/mds-http
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
#include <Update.h>
#include <cassert>
#include <cstdio>
#include <vector>

using Error = MDS_HTTP::Error;
using UploadMode = MDS_HTTP::UploadMode;

// Deliver a complete valid firmware image followed by the POST completion handler.
static void upload(MDS_HTTP& http) {
	auto& server = http.server();
	server.chunk(UPLOAD_FILE_START);
	server.chunk(UPLOAD_FILE_WRITE, 16);
	server.chunk(UPLOAD_FILE_END, 16);
	server.request(HTTP_POST);
}

// Verify optional routes, failure recovery, callbacks and update resource cleanup.
int main() {
	MDS_HTTP disabled(MDS_HTTP::Options{});
	assert(disabled.server().handlers.empty());
	MDS_HTTP http(MDS_HTTP::Options{UploadMode::Enabled});
	auto& server = http.server();
	assert(server.handlers.size() == 2);
	int completions = 0;
	std::vector<String> messages;
	http.setStatusCallback([&](const String& message) {
		// Capture messages without requiring a Serial device.
		messages.push_back(message);
	});
	http.setUploadCompleteCallback([&]() {
		// A successful HTTP response must already exist when the client schedules reboot.
		assert(server.code == 200);
		assert(server.body == "Update OK. Rebooting...\n");
		++completions;
	});
	bool networkReady = true;
	http.setNetworkReadyCallback([&]() {
		// Supply live connectivity for lifecycle and upload scenarios.
		return networkReady;
	});
	http.begin();
	messages.clear();
	server.request(HTTP_GET);
	assert(server.code == 200 && server.body.find("/upload") != String::npos);
	server.request(HTTP_POST);
	assert(server.code == 500 && completions == 0 && Update.begins == 0);
	assert(http.error() == Error::InvalidUpload);

	upload(http);
	assert(completions == 1 && http.error() == Error::None);
	assert(String(http.lastError()).empty());
	assert(messages.front() == "OTA: Missing or incomplete firmware upload");
	assert(messages.back() == "OTA: Success.");

	Update.beginOK = false;
	upload(http);
	assert(http.error() == Error::UploadStartFailed && completions == 1);
	assert(server.code == 500);
	Update.beginOK = true;
	Update.writeOK = false;
	upload(http);
	assert(http.error() == Error::UploadWriteFailed && completions == 1);
	assert(!Update.active);
	Update.writeOK = true;
	Update.endOK = false;
	upload(http);
	assert(http.error() == Error::UploadFinalizeFailed && completions == 1);
	Update.endOK = true;

	// Listener activity, page requests and callback registration preserve the failure.
	http.stop();
	http.begin();
	http.tick();
	server.request(HTTP_GET);
	assert(http.error() == Error::UploadFinalizeFailed);
	http.setStatusCallback(nullptr);
	assert(http.error() == Error::UploadFinalizeFailed);
	upload(http);
	assert(completions == 2 && http.error() == Error::None);

	server.chunk(UPLOAD_FILE_START);
	server.chunk(UPLOAD_FILE_END, 0);
	server.request(HTTP_POST);
	assert(http.error() == Error::InvalidUpload && completions == 2);
	server.chunk(UPLOAD_FILE_START);
	server.chunk(UPLOAD_FILE_WRITE, 16);
	server.request(HTTP_POST);
	assert(http.error() == Error::InvalidUpload && !Update.active);

	server.incoming.name = "other";
	server.chunk(UPLOAD_FILE_START);
	server.request(HTTP_POST);
	assert(http.error() == Error::InvalidUpload);
	server.incoming.name = "firmware";
	server.chunk(UPLOAD_FILE_START);
	server.chunk(UPLOAD_FILE_START);
	server.request(HTTP_POST);
	assert(http.error() == Error::InvalidUpload && completions == 2);

	// A later multipart file invalidates the request before firmware is finalized.
	const int endsBeforeMultipart = Update.ends;
	server.chunk(UPLOAD_FILE_START);
	server.chunk(UPLOAD_FILE_WRITE, 16);
	server.chunk(UPLOAD_FILE_END, 16);
	assert(Update.ends == endsBeforeMultipart);
	server.chunk(UPLOAD_FILE_START);
	server.request(HTTP_POST);
	assert(http.error() == Error::InvalidUpload && completions == 2);
	assert(Update.ends == endsBeforeMultipart);

	server.chunk(UPLOAD_FILE_START);
	server.chunk(UPLOAD_FILE_ABORTED);
	assert(http.error() == Error::UploadAborted && !Update.active);
	upload(http);
	assert(completions == 3 && http.error() == Error::None);
	server.chunk(UPLOAD_FILE_START);
	http.stop();
	assert(http.error() == Error::UploadAborted && !Update.active);
	http.begin();
	assert(server.handlers.size() == 2);
	upload(http);
	assert(completions == 4);

	// Repeating POST without another image cannot notify completion again.
	server.request(HTTP_POST);
	assert(completions == 4 && server.code == 500);
	http.setUploadCompleteCallback(nullptr);
	assert(http.error() == Error::InvalidUpload);
	upload(http);
	assert(http.error() == Error::None && completions == 4);

	// Losing readiness aborts a partial image once and preserves its error on reconnect.
	server.chunk(UPLOAD_FILE_START);
	assert(Update.active);
	const int abortsBeforeDisconnect = Update.aborts;
	networkReady = false;
	http.tick();
	assert(!http.running() && !Update.active);
	assert(http.error() == Error::UploadAborted && completions == 4);
	assert(Update.aborts == abortsBeforeDisconnect + 1);
	http.tick();
	assert(Update.aborts == abortsBeforeDisconnect + 1);
	networkReady = true;
	http.tick();
	assert(http.running() && http.error() == Error::UploadAborted);

	// Clearing the readiness source has the same cleanup behavior as network loss.
	server.chunk(UPLOAD_FILE_START);
	http.setNetworkReadyCallback(nullptr);
	http.tick();
	assert(!http.running() && !Update.active && http.error() == Error::UploadAborted);
	http.setNetworkReadyCallback([&]() {
		// Restore the application's connectivity source after clearing it.
		return networkReady;
	});
	http.tick();
	assert(http.running() && http.error() == Error::UploadAborted);

	const int beforeDestruction = Update.aborts;
	{
		MDS_HTTP partial(MDS_HTTP::Options{UploadMode::Enabled});
		partial.server().chunk(UPLOAD_FILE_START);
		assert(Update.active);
	}
	assert(Update.aborts == beforeDestruction + 1 && !Update.active);
	puts("PASS: HTTP uploads, failures, callback ordering, reconnect and partial-image cleanup.");
}
