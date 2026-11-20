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
#include <algorithm>

using State = MDS_HTTP::State;
using Error = MDS_HTTP::Error;
using UploadMode = MDS_HTTP::UploadMode;

// Deliver a complete valid firmware image followed by the POST completion handler.
static void upload(MDS_HTTP& http) {
	auto& server = http.server();
	server.chunk(UPLOAD_FILE_START);
	assert(http.running());
	assert(http.state() == (http.error() == Error::None ? State::Uploading : State::UploadComplete));
	server.chunk(UPLOAD_FILE_WRITE, 16);
	server.chunk(UPLOAD_FILE_END, 16);
	if (http.error() == Error::None) {
		assert(http.state() == State::Uploading);
	}
	server.request(HTTP_POST);
	assert(http.state() == State::UploadComplete && http.running());
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
		assert(http.state() == State::UploadComplete);
		assert(server.body == "Update OK. Rebooting...\n");
		++completions;
	});
	bool networkReady = true;
	http.setNetworkReadyCallback([&]() {
		// Supply live connectivity for lifecycle and upload scenarios.
		return networkReady;
	});
	http.begin();
	int updaterStartsAtLastCompletion = Update.begins;
	int uploadStartNotifications = 0;
	http.setStateChangeCallback([&](State previous, State current) {
		// Pre-upload notification must precede updater initialization and writes.
		assert(previous != current && http.state() == current);
		if (current == State::Uploading) {
			assert(!Update.active && Update.begins == updaterStartsAtLastCompletion);
			assert(http.error() == Error::None);
			++uploadStartNotifications;
		} else if (current == State::UploadComplete) {
			updaterStartsAtLastCompletion = Update.begins;
		}
	});
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
	assert(std::find(messages.begin(), messages.end(), "OTA: Success.") != messages.end());
	assert(messages.back() == "HTTP: Uploading -> UploadComplete");
	const int listenerStartsAfterUpload = server.starts;
	http.tick();
	assert(http.state() == State::UploadComplete && server.starts == listenerStartsAfterUpload);

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
	assert(http.state() == State::UploadComplete);
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

	std::vector<std::pair<State, Error>> transitions;
	http.setStatusCallback([&](const String& message) {
		// Applications can observe completion before a subsequent listener-state change.
		if (message.find(" -> ") != String::npos) {
			transitions.push_back({http.state(), http.error()});
		}
	});

	// Losing readiness aborts a partial image once and preserves its error on reconnect.
	server.chunk(UPLOAD_FILE_START);
	assert(Update.active);
	const int abortsBeforeDisconnect = Update.aborts;
	networkReady = false;
	http.tick();
	assert(!http.running() && !Update.active);
	assert(http.state() == State::WaitingForNetwork);
	assert(transitions.size() == 3);
	assert(transitions[0].first == State::Uploading);
	assert(transitions[1].first == State::UploadComplete && transitions[1].second == Error::UploadAborted);
	assert(transitions[2].first == State::WaitingForNetwork);
	assert(http.error() == Error::UploadAborted && completions == 4);
	assert(Update.aborts == abortsBeforeDisconnect + 1);
	http.tick();
	assert(Update.aborts == abortsBeforeDisconnect + 1);
	networkReady = true;
	http.tick();
	assert(http.running() && http.error() == Error::UploadAborted);
	assert(http.state() == State::Listening);

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
	assert(http.state() == State::Listening);

	// Explicit shutdown reports the interrupted upload before committing Stopped.
	transitions.clear();
	server.chunk(UPLOAD_FILE_START);
	http.stop();
	assert(http.state() == State::Stopped && !http.running());
	assert(transitions.size() == 3);
	assert(transitions[1].first == State::UploadComplete && transitions[1].second == Error::UploadAborted);
	assert(transitions[2].first == State::Stopped);
	const auto stoppedTransitions = transitions.size();
	http.tick();
	http.stop();
	assert(transitions.size() == stoppedTransitions);

	assert(uploadStartNotifications > 0);
	http.setStateChangeCallback(nullptr);

	const int beforeDestruction = Update.aborts;
	{
		MDS_HTTP partial(MDS_HTTP::Options{UploadMode::Enabled});
		partial.setNetworkReadyCallback([]() {
			// Supply connectivity before starting the destructor-cleanup scenario.
			return true;
		});
		partial.begin();
		partial.server().chunk(UPLOAD_FILE_START);
		assert(Update.active);
	}
	assert(Update.aborts == beforeDestruction + 1 && !Update.active);
	puts("PASS: HTTP uploads, failures, callback ordering, reconnect and partial-image cleanup.");
}
