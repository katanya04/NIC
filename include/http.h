#ifndef HTTP_H
#define HTTP_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "tcp.h"
#include "hal.h"

#define HTTP_MAX_REQUEST_SIZE   4096
#define HTTP_MAX_RESPONSE_SIZE  8192
#define HTTP_MAX_PATH_SIZE      256
#define HTTP_MAX_HEADERS        32
#define HTTP_MAX_HEADER_NAME    64
#define HTTP_MAX_HEADER_VALUE   256

typedef enum {
    HTTP_METHOD_GET,
    HTTP_METHOD_POST,
    HTTP_METHOD_HEAD,
    HTTP_METHOD_PUT,
    HTTP_METHOD_DELETE,
    HTTP_METHOD_OPTIONS,
    HTTP_METHOD_UNKNOWN
} http_method_t;

typedef struct {
    char name[HTTP_MAX_HEADER_NAME];
    char value[HTTP_MAX_HEADER_VALUE];
} http_header_t;

typedef struct {
    http_method_t method;
    char path[HTTP_MAX_PATH_SIZE];
    char version[16];
    http_header_t headers[HTTP_MAX_HEADERS];
    int header_count;
    const uint8_t *body;
    size_t body_len;
    size_t content_length;
} http_request_t;

typedef struct {
    int status_code;
    char status_text[32];
    http_header_t headers[HTTP_MAX_HEADERS];
    int header_count;
    const char *body;
    size_t body_len;
} http_response_t;


typedef void (*http_request_handler_t)(const http_request_t *request, 
                                        http_response_t *response,
                                        void *user_data);

void http_init(http_request_handler_t handler, void *user_data);


void http_handler(struct device_handle *dev, tcp_conn_t *conn,
                  const uint8_t *payload, int payload_len);

bool http_parse_request(const uint8_t *data, size_t len, http_request_t *request);

const char *http_get_header(const http_request_t *request, const char *name);

void http_response_init(http_response_t *response, int status_code, const char *status_text);

void http_response_add_header(http_response_t *response, const char *name, const char *value);

void http_response_set_body(http_response_t *response, const char *body, size_t body_len);

int http_serialize_response(const http_response_t *response, uint8_t *buffer, size_t buffer_size);

int http_send_response(struct device_handle *dev, tcp_conn_t *conn, 
                       const http_response_t *response);

void http_send_text(struct device_handle *dev, tcp_conn_t *conn,
                    int status_code, const char *status_text, const char *body);

void http_send_html(struct device_handle *dev, tcp_conn_t *conn,
                    int status_code, const char *status_text, const char *html);

void http_send_404(struct device_handle *dev, tcp_conn_t *conn);

void http_send_500(struct device_handle *dev, tcp_conn_t *conn);

#endif