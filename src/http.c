#include "http.h"
#include "tcp.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

static http_request_handler_t g_request_handler = NULL;
static void *g_user_data = NULL;

static const char *find_line_end(const char *start, const char *end) {
    while (start < end - 1) {
        if (start[0] == '\r' && start[1] == '\n') {
            return start;
        }
        start++;
    }
    return NULL;
}

static http_method_t parse_method(const char *method_str) {
    if (strncmp(method_str, "GET", 3) == 0) return HTTP_METHOD_GET;
    if (strncmp(method_str, "POST", 4) == 0) return HTTP_METHOD_POST;
    if (strncmp(method_str, "HEAD", 4) == 0) return HTTP_METHOD_HEAD;
    if (strncmp(method_str, "PUT", 3) == 0) return HTTP_METHOD_PUT;
    if (strncmp(method_str, "DELETE", 6) == 0) return HTTP_METHOD_DELETE;
    if (strncmp(method_str, "OPTIONS", 7) == 0) return HTTP_METHOD_OPTIONS;
    return HTTP_METHOD_UNKNOWN;
}

static const char *method_to_string(http_method_t method) {
    switch (method) {
        case HTTP_METHOD_GET: return "GET";
        case HTTP_METHOD_POST: return "POST";
        case HTTP_METHOD_HEAD: return "HEAD";
        case HTTP_METHOD_PUT: return "PUT";
        case HTTP_METHOD_DELETE: return "DELETE";
        case HTTP_METHOD_OPTIONS: return "OPTIONS";
        default: return "UNKNOWN";
    }
}

static void trim(char *str) {
    char *end;
    while(isspace((unsigned char)*str)) str++;
    if(*str == 0) return;
    end = str + strlen(str) - 1;
    while(end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
}

void http_init(http_request_handler_t handler, void *user_data) {
    g_request_handler = handler;
    g_user_data = user_data;
    printf("HTTP: Server initialized\n");
}

bool http_parse_request(const uint8_t *data, size_t len, http_request_t *request) {
    if (!data || !request || len == 0) {
        return false;
    }
    
    memset(request, 0, sizeof(http_request_t));
    
    const char *start = (const char *)data;
    const char *end = start + len;
    const char *line_end;
    
    line_end = find_line_end(start, end);
    if (!line_end) {
        printf("HTTP: No valid request line found\n");
        return false;
    }
    
    char request_line[512];
    size_t line_len = line_end - start;
    if (line_len >= sizeof(request_line)) {
        printf("HTTP: Request line too long\n");
        return false;
    }
    
    memcpy(request_line, start, line_len);
    request_line[line_len] = '\0';
    
    char method_str[16] = {0};
    char path[HTTP_MAX_PATH_SIZE] = {0};
    char version[16] = {0};
    
    int parsed = sscanf(request_line, "%15s %255s %15s", method_str, path, version);
    if (parsed != 3) {
        printf("HTTP: Failed to parse request line: %s\n", request_line);
        return false;
    }
    
    request->method = parse_method(method_str);
    strncpy(request->path, path, HTTP_MAX_PATH_SIZE - 1);
    strncpy(request->version, version, sizeof(request->version) - 1);
    
    printf("HTTP: %s %s %s\n", method_str, request->path, request->version);

    start = line_end + 2;
    request->header_count = 0;
    
    while (start < end && request->header_count < HTTP_MAX_HEADERS) {
        line_end = find_line_end(start, end);
        if (!line_end) {
            break;
        }
        
        line_len = line_end - start;
        
        if (line_len == 0) {
            start = line_end + 2;
            break;
        }
        
        char header_line[HTTP_MAX_HEADER_NAME + HTTP_MAX_HEADER_VALUE + 2];
        if (line_len >= sizeof(header_line)) {
            start = line_end + 2;
            continue;
        }
        
        memcpy(header_line, start, line_len);
        header_line[line_len] = '\0';
        
        char *colon = strchr(header_line, ':');
        if (colon) {
            *colon = '\0';
            char *name = header_line;
            char *value = colon + 1;
            
            trim(name);
            trim(value);
            
            http_header_t *header = &request->headers[request->header_count];
            strncpy(header->name, name, HTTP_MAX_HEADER_NAME - 1);
            strncpy(header->value, value, HTTP_MAX_HEADER_VALUE - 1);
            request->header_count++;

            if (strcasecmp(name, "Content-Length") == 0) {
                request->content_length = atoi(value);
            }
        }
        
        start = line_end + 2;
    }
    
    if (start < end) {
        request->body = (const uint8_t *)start;
        request->body_len = end - start;
    }
    
    return true;
}

const char *http_get_header(const http_request_t *request, const char *name) {
    if (!request || !name) {
        return NULL;
    }
    
    for (int i = 0; i < request->header_count; i++) {
        if (strcasecmp(request->headers[i].name, name) == 0) {
            return request->headers[i].value;
        }
    }
    
    return NULL;
}

void http_response_init(http_response_t *response, int status_code, const char *status_text) {
    if (!response) return;
    
    memset(response, 0, sizeof(http_response_t));
    response->status_code = status_code;
    strncpy(response->status_text, status_text, sizeof(response->status_text) - 1);
}

void http_response_add_header(http_response_t *response, const char *name, const char *value) {
    if (!response || !name || !value) return;
    if (response->header_count >= HTTP_MAX_HEADERS) return;
    
    http_header_t *header = &response->headers[response->header_count];
    strncpy(header->name, name, HTTP_MAX_HEADER_NAME - 1);
    strncpy(header->value, value, HTTP_MAX_HEADER_VALUE - 1);
    response->header_count++;
}

void http_response_set_body(http_response_t *response, const char *body, size_t body_len) {
    if (!response) return;
    response->body = body;
    response->body_len = body_len;
}

int http_serialize_response(const http_response_t *response, uint8_t *buffer, size_t buffer_size) {
    if (!response || !buffer || buffer_size == 0) {
        return -1;
    }
    
    int offset = 0;

    offset += snprintf((char *)buffer + offset, buffer_size - offset,
                      "HTTP/1.1 %d %s\r\n",
                      response->status_code, response->status_text);
    
    if (offset >= buffer_size) return -1;
    
    for (int i = 0; i < response->header_count; i++) {
        offset += snprintf((char *)buffer + offset, buffer_size - offset,
                          "%s: %s\r\n",
                          response->headers[i].name, response->headers[i].value);
        if (offset >= buffer_size) return -1;
    }
    
    offset += snprintf((char *)buffer + offset, buffer_size - offset, "\r\n");
    if (offset >= buffer_size) return -1;

    if (response->body && response->body_len > 0) {
        if (offset + response->body_len > buffer_size) {
            return -1;
        }
        memcpy(buffer + offset, response->body, response->body_len);
        offset += response->body_len;
    }
    
    return offset;
}

int http_send_response(struct device_handle *dev, tcp_conn_t *conn, 
                       const http_response_t *response) {
    if (!dev || !conn || !response) {
        return -1;
    }
    
    uint8_t buffer[HTTP_MAX_RESPONSE_SIZE];
    int len = http_serialize_response(response, buffer, sizeof(buffer));
    
    if (len < 0) {
        printf("HTTP: Failed to serialize response\n");
        return -1;
    }
    
    printf("HTTP: Sending %d bytes response (status %d)\n", len, response->status_code);
    
    int result = tcp_send(dev, conn->remote_ip, conn->local_port, conn->remote_port,
                         conn->seq, conn->ack, 
                         TCP_FLAG_PSH | TCP_FLAG_ACK, 
                         buffer, len);
    
    if (result == 0) {
        conn->seq += len;
    }
    
    return result;
}

void http_send_text(struct device_handle *dev, tcp_conn_t *conn,
                    int status_code, const char *status_text, const char *body) {
    http_response_t response;
    http_response_init(&response, status_code, status_text);
    
    if (body) {
        char content_length[32];
        snprintf(content_length, sizeof(content_length), "%zu", strlen(body));
        http_response_add_header(&response, "Content-Type", "text/plain");
        http_response_add_header(&response, "Content-Length", content_length);
        http_response_set_body(&response, body, strlen(body));
    } else {
        http_response_add_header(&response, "Content-Length", "0");
    }
    
    http_response_add_header(&response, "Connection", "close");
    http_send_response(dev, conn, &response);
}

void http_send_html(struct device_handle *dev, tcp_conn_t *conn,
                    int status_code, const char *status_text, const char *html) {
    http_response_t response;
    http_response_init(&response, status_code, status_text);
    
    if (html) {
        char content_length[32];
        snprintf(content_length, sizeof(content_length), "%zu", strlen(html));
        http_response_add_header(&response, "Content-Type", "text/html; charset=utf-8");
        http_response_add_header(&response, "Content-Length", content_length);
        http_response_set_body(&response, html, strlen(html));
    } else {
        http_response_add_header(&response, "Content-Length", "0");
    }
    
    http_response_add_header(&response, "Connection", "close");
    http_send_response(dev, conn, &response);
}

void http_send_404(struct device_handle *dev, tcp_conn_t *conn) {
    const char *html = 
        "<!DOCTYPE html>\n"
        "<html>\n"
        "<head><title>404 Not Found</title></head>\n"
        "<body>\n"
        "<h1>404 Not Found</h1>\n"
        "<p>The requested resource was not found on this server.</p>\n"
        "</body>\n"
        "</html>\n";
    
    http_send_html(dev, conn, 404, "Not Found", html);
}

void http_send_500(struct device_handle *dev, tcp_conn_t *conn) {
    const char *html = 
        "<!DOCTYPE html>\n"
        "<html>\n"
        "<head><title>500 Internal Server Error</title></head>\n"
        "<body>\n"
        "<h1>500 Internal Server Error</h1>\n"
        "<p>An error occurred while processing your request.</p>\n"
        "</body>\n"
        "</html>\n";
    
    http_send_html(dev, conn, 500, "Internal Server Error", html);
}

void http_handler(struct device_handle *dev, tcp_conn_t *conn,
                  const uint8_t *payload, int payload_len) {
    if (!dev || !conn || !payload || payload_len <= 0) {
        return;
    }
    
    printf("HTTP: Processing request (%d bytes)\n", payload_len);
    
    http_request_t request;
    if (!http_parse_request(payload, payload_len, &request)) {
        printf("HTTP: Failed to parse request\n");
        http_send_500(dev, conn);
        return;
    }
    
    if (g_request_handler) {
        http_response_t response;
        http_response_init(&response, 200, "OK");
        
        g_request_handler(&request, &response, g_user_data);
        http_send_response(dev, conn, &response);
    } else {
        printf("HTTP: No handler registered, sending default response\n");
        
        const char *html = 
            "<!DOCTYPE html>\n"
            "<html>\n"
            "<head><title>HTTP Server</title></head>\n"
            "<body>\n"
            "<h1>Welcome to NIC HTTP Server</h1>\n"
            "<p>Request received!</p>\n"
            "<ul>\n"
            "<li>Method: %s</li>\n"
            "<li>Path: %s</li>\n"
            "<li>Version: %s</li>\n"
            "</ul>\n"
            "</body>\n"
            "</html>\n";
        
        char body[1024];
        snprintf(body, sizeof(body), html, 
                method_to_string(request.method), 
                request.path, 
                request.version);
        
        http_send_html(dev, conn, 200, "OK", body);
    }
}