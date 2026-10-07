#ifndef WIFI_HTTP_SERVER_H
#define WIFI_HTTP_SERVER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// The request contains the raw HTTP headers, including the final CRLF pair.
// It is null-terminated, but request_length does not include that terminator.
// The pointer is only valid until the callback returns. Keep callbacks short:
// they run in the lwIP background context.
typedef void (*wifi_http_request_callback_t)(const char *request,
                                             size_t request_length);

// Registers the function called once whenever a complete request header is
// received. Pass NULL to stop receiving callbacks.
void wifi_http_server_register_request_callback(
    wifi_http_request_callback_t callback);

// Connects in station mode and starts a plain HTTP server. The Wi-Fi driver
// continues running in the background after this function returns.
bool wifi_http_server_start(const char *ssid, const char *password, uint16_t port);

#endif
