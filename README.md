# MDS_HTTP

A lightweight wrapper for starting, servicing, and stopping a web server. Applications define their own routes.

## Requirements

- **Platforms:** ESP32 and ESP8266.
- **Dependencies:** WebServer (ESP32) or ESP8266WebServer (ESP8266), supplied by the board package.

## Installation

Place the `MDS_HTTP` folder in your Arduino sketchbook's `libraries` directory. Install the required board package and dependencies using the Arduino IDE.

## Example

Serve a greeting at `/` using the board package's web server. This example uses **MDS_WiFi** and **WiFiManager 2.0.17 or later** for connectivity.

With no saved Wi-Fi credentials, join the `Web Demo <MAC>` access point (password `12345678`) and enter your network details in its portal. Once connected, open the URL printed on Serial from a browser on the same network. HTTP starts and suspends its listener through a Wi-Fi readiness callback; registered routes remain available after reconnection.

```cpp
#include <MDS_WiFi.h>
#include <MDS_HTTP.h>

MDS_WiFi wifi;
MDS_HTTP http;  // Port 80 by default.

// Register routes and connectivity before enabling HTTP.
void setup() {
    Serial.begin(115200);

    // Register application routes before starting the server.
    http.server().on("/", HTTP_GET, []() {
        http.server().send(200, "text/plain", "Hello from MDS_HTTP!\n");
    });

    MDS_WiFi::Options options;

    options.accessPointPrefix = "Web Demo";
    options.connectionPolicy =
        MDS_WiFi::ConnectionPolicy::RetrySavedNetworkAsync;

    wifi.begin(options);

    http.setNetworkReadyCallback([]() {
        // Let the connection manager define station and IP readiness.
        return wifi.ready();
    });
    http.setStatusCallback([](const String& message) {
        // Report listener transitions without managing its lifecycle here.
        Serial.println(message);
        if (http.running()) {
            Serial.print("Open http://");
            Serial.println(WiFi.localIP());
        }
    });
    http.begin();
}

// Service connectivity and HTTP while other application work continues.
void loop() {
    wifi.tick();
    http.tick();

    // Do other application work here.
}
```

## Optional firmware upload

The default `MDS_HTTP http;` creates no upload routes and does not link the upload
implementation. Opt in at construction, before registering application routes:

```cpp
MDS_HTTP http(MDS_HTTP::Options{MDS_HTTP::UploadMode::Enabled});
bool shouldReboot = false;

// Register application-owned output and reboot policy before enabling HTTP.
void configureUploadCallbacks() {
    http.setStatusCallback([](const String& message) {
        // Send library diagnostics to the application output.
        Serial.println(message);
    });

    http.setUploadCompleteCallback([]() {
        // Schedule a reboot in loop() after the successful response has been sent.
        shouldReboot = true;
    });
}
```

This registers GET and POST `/upload`, including the browser form. Upload one
firmware binary in the `firmware` multipart field. The board updater validates
the image before the success response. The library does not reboot; the
application handles `shouldReboot` after servicing HTTP. Both callback setters
accept `nullptr` to clear their callback. With no status callback, uploads are
silent. Register a network-ready callback and call begin() to enable listening.

`error()` returns `Error::None`, `InvalidUpload`, `UploadStartFailed`,
`UploadWriteFailed`, `UploadFinalizeFailed`, or `UploadAborted`. `lastError()`
returns diagnostic text. A new upload clears the previous failure; ordinary
requests, callback registration and listener restarts preserve it. Failed,
missing, empty or incomplete uploads never invoke the completion callback.
Stopping or destroying the server cancels an active upload; routes remain
registered across `stop()` and `begin()`.

## Documentation

Detailed design and API documentation: [MDS_HTTP](https://home.marcspencer.com/mds-http).

## License

Firmware and software: **MPL-2.0**. Documentation and website: **CC BY-SA 4.0**. Any hardware design files and Gerbers: **CERN-OHL-W-2.0**.

## Contributions

Contributions require a Developer Certificate of Origin (DCO) sign-off. Use `git commit -s` to add a `Signed-off-by` line to each contribution commit.

## Readiness and explicit shutdown

Register `setNetworkReadyCallback()` with a quick, nonblocking `bool()` callback.
`begin()` enables HTTP and checks readiness immediately; `tick()` checks it again
before dispatching requests. An unset callback means unavailable. Passing
`nullptr` clears the source; the next `tick()` suspends an active listener.
Captured objects must outlive their callback registrations. Missing readiness is
an ordinary wait and does not replace existing errors.

Network loss closes the listener and cancels any in-progress firmware upload;
reconnection starts listening again with the registered routes intact. Aborting
an active upload records `UploadAborted`, as explicit shutdown does. Listener
transitions emit `HTTP: listening` or `HTTP: stopped` through the status callback.
Repeated waits and unchanged readiness do not repeat those messages.

`stop()` explicitly disables HTTP. Further ticks or callback changes cannot
restart it until `begin()` is called again. This keeps reset/reboot shutdown
separate from temporary network loss. `running()` reports the actual listener,
so it remains false while an enabled service is waiting for connectivity.
