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
#include "Arduino.h"
#include <functional>
#include <map>
#include <utility>

enum HTTPMethod {
	HTTP_GET,
	HTTP_POST
};

enum HTTPUploadStatus {
	UPLOAD_FILE_START,
	UPLOAD_FILE_WRITE,
	UPLOAD_FILE_END,
	UPLOAD_FILE_ABORTED
};

struct HTTPUpload {
	HTTPUploadStatus status = UPLOAD_FILE_START;
	String name = "firmware";
	size_t currentSize = 0;
	size_t totalSize = 0;
	uint8_t buf[16]{};
};

class WebServer {
public:
	using Handler = std::function<void()>;
	std::map<std::pair<String, HTTPMethod>, Handler> handlers;
	Handler stream;
	HTTPUpload incoming;
	int starts = 0;
	int stops = 0;
	int ticks = 0;
	int code = 0;
	String body;

	// Construct a fake server without opening a socket.
	explicit WebServer(uint16_t) {
	}

	// Register the request completion and optional stream handlers.
	void on(const char* path, HTTPMethod method, Handler handler, Handler upload = nullptr) {
		handlers[{path, method}] = handler;
		if (upload) {
			stream = upload;
		}
	}

	// Expose the current multipart chunk.
	HTTPUpload& upload() {
		return incoming;
	}

	// Record the response before the completion callback is invoked.
	void send(int status, const char*, const String& text) {
		code = status;
		body = text;
	}

	// Count listener starts without network activity.
	void begin() {
		++starts;
	}

	// Count listener shutdowns.
	void stop() {
		++stops;
	}

	// Count request service calls.
	void handleClient() {
		++ticks;
	}

	// Deliver an upload event through the registered route.
	void chunk(HTTPUploadStatus status, size_t size = 0) {
		incoming.status = status;
		incoming.currentSize = size;
		incoming.totalSize = size;
		stream();
	}

	// Deliver a complete request through the registered route.
	void request(HTTPMethod method) {
		handlers.at({"/upload", method})();
	}
};
