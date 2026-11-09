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
#include <cstddef>
#include <cstdint>
constexpr size_t UPDATE_SIZE_UNKNOWN = size_t(-1);

struct FakeUpdate {
	bool beginOK = true;
	bool writeOK = true;
	bool endOK = true;
	bool active = false;
	int begins = 0;
	int writes = 0;
	int ends = 0;
	int aborts = 0;

	// Simulate update initialization and retain its active state.
	bool begin(size_t) {
		++begins;
		active = beginOK;
		return beginOK;
	}

	// Simulate a full write or a short write.
	size_t write(uint8_t*, size_t size) {
		++writes;
		return writeOK ? size : 0;
	}

	// Simulate validation and image finalization.
	bool end(bool finalize) {
#if defined(ARDUINO_ARCH_ESP8266)
		if (!finalize) {
			++aborts;
			active = false;
			return false;
		}
#else
		(void)finalize;
#endif
		++ends;
		active = false;
		return endOK;
	}

	// Discard a partial image without committing it.
	void abort() {
		++aborts;
		active = false;
	}
};

inline FakeUpdate Update;
