#!/bin/sh
# Run server lifecycle and upload tests for both supported platform branches.
set -eu
TEST_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
TEST_BUILD=$(mktemp -d)
trap 'rm -rf "$TEST_BUILD"' EXIT HUP INT TERM
for test_arch in ESP32 ESP8266; do
    c++ -std=c++17 -Wall -Wextra -Werror -DARDUINO_ARCH_$test_arch \
        -I"$TEST_DIR/host" -I"$TEST_DIR/../src" \
        "$TEST_DIR/basic.cpp" "$TEST_DIR/../src/MDS_HTTP.cpp" -o "$TEST_BUILD/basic-$test_arch"
    "$TEST_BUILD/basic-$test_arch"
    c++ -std=c++17 -Wall -Wextra -Werror -DARDUINO_ARCH_$test_arch \
        -I"$TEST_DIR/host" -I"$TEST_DIR/../src" \
        "$TEST_DIR/readiness.cpp" "$TEST_DIR/../src/MDS_HTTP.cpp" -o "$TEST_BUILD/readiness-$test_arch"
    "$TEST_BUILD/readiness-$test_arch"
    c++ -std=c++17 -Wall -Wextra -Werror -DARDUINO_ARCH_$test_arch \
        -I"$TEST_DIR/host" -I"$TEST_DIR/../src" \
        "$TEST_DIR/upload.cpp" "$TEST_DIR/../src/MDS_HTTP.cpp" \
        "$TEST_DIR/../src/MDS_HTTPUpload.cpp" -o "$TEST_BUILD/upload-$test_arch"
    "$TEST_BUILD/upload-$test_arch"
done
