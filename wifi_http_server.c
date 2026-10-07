/*
 * Disclaimer: This file was generated with GPT 6 Sol and Claude Opus 5.5 in
 * like 5 total prompts because I hate dealing w/ networking in *non-embedded*
 * contexts, so like hell would I deal with it in embedded.
 * And fuck dealing with DNS myself. Sucks balls.
 */

#include "wifi_http_server.h"

#include <stdio.h>
#include <stdlib.h>

#include "lwip/apps/mdns.h"
#include "lwip/ip_addr.h"
#include "lwip/netif.h"
#include "lwip/pbuf.h"
#include "lwip/tcp.h"
#include "pico/cyw43_arch.h"

#define WIFI_CONNECT_TIMEOUT_MS 30000
#define MAX_REQUEST_HEADER_SIZE 2048

#ifndef MDNS_HOSTNAME
#define MDNS_HOSTNAME "infrared"
#endif

typedef struct {
    struct tcp_pcb *pcb;
    unsigned int header_terminator_bytes;
    size_t request_length;
    char request[MAX_REQUEST_HEADER_SIZE + 1];
    bool response_sent;
} http_connection_t;

static struct tcp_pcb *server_pcb;
static wifi_http_request_callback_t request_callback;

static const char response[] =
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: text/plain\r\n"
    "Content-Length: 3\r\n"
    "Connection: close\r\n"
    "\r\n"
    "OK\n";

void wifi_http_server_register_request_callback(
    wifi_http_request_callback_t callback) {
    request_callback = callback;
}

static err_t close_connection(http_connection_t *connection) {
    if (connection == NULL) {
        return ERR_OK;
    }

    err_t result = ERR_OK;
    if (connection->pcb != NULL) {
        tcp_arg(connection->pcb, NULL);
        tcp_recv(connection->pcb, NULL);
        tcp_sent(connection->pcb, NULL);
        tcp_err(connection->pcb, NULL);
        if (tcp_close(connection->pcb) != ERR_OK) {
            tcp_abort(connection->pcb);
            result = ERR_ABRT;
        }
    }

    free(connection);
    return result;
}

static bool append_request_data(http_connection_t *connection,
                                const unsigned char *data, u16_t length) {
    static const char terminator[] = "\r\n\r\n";

    for (u16_t i = 0; i < length; ++i) {
        if (connection->request_length == MAX_REQUEST_HEADER_SIZE) {
            return false;
        }

        connection->request[connection->request_length++] = (char)data[i];
        if (data[i] == (unsigned char)terminator[connection->header_terminator_bytes]) {
            ++connection->header_terminator_bytes;
            if (connection->header_terminator_bytes == sizeof(terminator) - 1) {
                connection->request[connection->request_length] = '\0';
                return true;
            }
        } else {
            connection->header_terminator_bytes = data[i] == '\r' ? 1u : 0u;
        }
    }

    return false;
}

static err_t response_acked(void *arg, struct tcp_pcb *pcb, u16_t length) {
    (void)pcb;
    (void)length;
    return close_connection((http_connection_t *)arg);
}

static void connection_error(void *arg, err_t error) {
    (void)error;
    http_connection_t *connection = (http_connection_t *)arg;
    // lwIP has already freed the PCB when this callback runs.
    connection->pcb = NULL;
    close_connection(connection);
}

static err_t receive_request(void *arg, struct tcp_pcb *pcb, struct pbuf *packet,
                             err_t error) {
    http_connection_t *connection = (http_connection_t *)arg;

    if (packet == NULL) {
        return close_connection(connection);
    }
    if (error != ERR_OK) {
        pbuf_free(packet);
        return error;
    }

    bool headers_complete = false;
    for (const struct pbuf *chunk = packet; chunk != NULL; chunk = chunk->next) {
        if (!headers_complete) {
            headers_complete = append_request_data(
                connection, (const unsigned char *)chunk->payload, chunk->len);
        }
    }

    tcp_recved(pcb, packet->tot_len);
    pbuf_free(packet);

    if (!headers_complete &&
        connection->request_length == MAX_REQUEST_HEADER_SIZE) {
        printf("HTTP request headers exceeded %u bytes\n",
               MAX_REQUEST_HEADER_SIZE);
        return close_connection(connection);
    }

    if (headers_complete && !connection->response_sent) {
        connection->response_sent = true;
        if (request_callback != NULL) {
            request_callback(connection->request, connection->request_length);
        }
        err_t write_error = tcp_write(pcb, response, sizeof(response) - 1,
                                      TCP_WRITE_FLAG_COPY);
        if (write_error != ERR_OK) {
            printf("HTTP response write failed: %d\n", write_error);
            return close_connection(connection);
        }
        tcp_output(pcb);
    }

    return ERR_OK;
}

static err_t accept_connection(void *arg, struct tcp_pcb *client_pcb, err_t error) {
    (void)arg;

    if (error != ERR_OK || client_pcb == NULL) {
        return ERR_VAL;
    }

    http_connection_t *connection = calloc(1, sizeof(*connection));
    if (connection == NULL) {
        tcp_abort(client_pcb);
        return ERR_ABRT;
    }

    connection->pcb = client_pcb;
    tcp_arg(client_pcb, connection);
    tcp_recv(client_pcb, receive_request);
    tcp_sent(client_pcb, response_acked);
    tcp_err(client_pcb, connection_error);
    return ERR_OK;
}

bool wifi_http_server_start(const char *ssid, const char *password, uint16_t port) {
    if (ssid == NULL || password == NULL || ssid[0] == '\0') {
        printf("Wi-Fi credentials are missing\n");
        return false;
    }
    if (cyw43_arch_init() != 0) {
        printf("CYW43 initialization failed\n");
        return false;
    }

    cyw43_arch_enable_sta_mode();
    printf("Connecting to Wi-Fi SSID '%s'...\n", ssid);
    int connect_error = cyw43_arch_wifi_connect_timeout_ms(
        ssid, password, CYW43_AUTH_WPA2_AES_PSK, WIFI_CONNECT_TIMEOUT_MS);
    if (connect_error != 0) {
        printf("Wi-Fi connection failed: %d\n", connect_error);
        cyw43_arch_deinit();
        return false;
    }

    cyw43_arch_lwip_begin();
    struct tcp_pcb *pcb = tcp_new_ip_type(IPADDR_TYPE_ANY);
    err_t bind_error = pcb == NULL ? ERR_MEM : tcp_bind(pcb, IP_ANY_TYPE, port);
    if (bind_error == ERR_OK) {
        server_pcb = tcp_listen_with_backlog(pcb, 2);
    }
    if (server_pcb != NULL) {
        tcp_accept(server_pcb, accept_connection);
        mdns_resp_init();
        err_t mdns_error = mdns_resp_add_netif(netif_default, MDNS_HOSTNAME);
        if (mdns_error == ERR_OK) {
            mdns_resp_add_service(netif_default, MDNS_HOSTNAME, "_http",
                                  DNSSD_PROTO_TCP, port, NULL, NULL);
        } else {
            printf("mDNS registration failed: %d\n", mdns_error);
        }
    }
    cyw43_arch_lwip_end();

    if (server_pcb == NULL) {
        printf("Could not listen on HTTP port %u (lwIP error %d)\n", port,
               bind_error);
        if (pcb != NULL && bind_error != ERR_OK) {
            cyw43_arch_lwip_begin();
            tcp_close(pcb);
            cyw43_arch_lwip_end();
        }
        cyw43_arch_deinit();
        return false;
    }

    printf("Wi-Fi connected: http://%s:%u/ (http://%s.local:%u/)\n",
           ip4addr_ntoa(netif_ip4_addr(netif_default)), port, MDNS_HOSTNAME,
           port);
    return true;
}
