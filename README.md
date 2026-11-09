# MDS_HTTP

A lightweight wrapper for starting, servicing, and stopping a web server. Applications define their own routes.

## Requirements

- **Platforms:** ESP32 and ESP8266.
- **Dependencies:** WebServer (ESP32) or ESP8266WebServer (ESP8266), supplied by the board package.

## Installation

Place the `MDS_HTTP` folder in your Arduino sketchbook's `libraries` directory. Install the required board package and dependencies using the Arduino IDE.

## Example

Serve a greeting at `/` using the board package's web server. This example uses **MDS_WiFi** and **WiFiManager 2.0.17 or later** for connectivity.

With no saved Wi-Fi credentials, join the `Web Demo <MAC>` access point (password `12345678`) and enter your network details in its portal. Once connected, open the URL printed on Serial from a browser on the same network. The application starts and stops HTTP with Wi-Fi readiness; registered routes remain available after reconnection.

```cpp
#include <MDS_WiFi.h>
#include <MDS_HTTP.h>

MDS_WiFi wifi;
MDS_HTTP http;  // Port 80 by default.

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
}

void loop() {
    wifi.tick();

    if (wifi.ready()) {
        if (!http.running()) {
            http.begin();

            Serial.print("Open http://");
            Serial.println(WiFi.localIP());
        }
    } else {
        http.stop();
    }

    http.tick();  // Service incoming requests.

    // Do other application work here.
}
```

## Optional firmware upload

The default `MDS_HTTP http;` creates no upload routes and does not link the upload
implementation. Opt in at construction, before registering application routes:

```cpp
MDS_HTTP http(MDS_HTTP::Options{MDS_HTTP::UploadMode::Enabled});
bool shouldReboot = false;

// Register application-owned output and reboot policy before calling begin().
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
silent. Listening and Wi-Fi readiness remain application responsibilities.

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
