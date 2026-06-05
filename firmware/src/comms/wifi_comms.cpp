/**
 * ============================================================================
 * OpenPlotter — WiFi Communication Implementation (ESP32)
 * ============================================================================
 *
 * Provides:
 *   - WiFi AP mode (creates its own network for initial setup)
 *   - WiFi STA mode (connects to user's home network)
 *   - mDNS (accessible at openplotter.local)
 *   - Async WebSocket server for real-time bidirectional G-code streaming
 *   - REST API endpoints for file upload and settings
 *   - Embedded web UI served from LittleFS
 *
 * ============================================================================
 */

#ifdef HAS_WIFI
#ifndef BRIDGE_MODE  // Full WiFi mode — not bridge mode

#include "wifi_comms.h"
#include "../utils/logger.h"

#include <WiFi.h>
#include <ESPmDNS.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

// ── Static server instances ─────────────────────────────────────────────────
static AsyncWebServer* _server = nullptr;
static AsyncWebSocket* _ws = nullptr;

// ── WebSocket receive buffer ────────────────────────────────────────────────
static char _wsRxBuffer[GCODE_LINE_MAX_LENGTH + 1];
static volatile bool _wsLineReady = false;

// ── WebSocket event handler ─────────────────────────────────────────────────
static void onWsEvent(AsyncWebSocket* server, AsyncWebSocketClient* client,
                       AwsEventType type, void* arg, uint8_t* data, size_t len) {
    switch (type) {
        case WS_EVT_CONNECT:
            LOG_INFO("WiFi: WebSocket client #%u connected from %s",
                     client->id(), client->remoteIP().toString().c_str());
            client->text("{\"msg\":\"Connected to OpenPlotter\"}");
            break;

        case WS_EVT_DISCONNECT:
            LOG_INFO("WiFi: WebSocket client #%u disconnected", client->id());
            break;

        case WS_EVT_DATA: {
            AwsFrameInfo* info = (AwsFrameInfo*)arg;
            if (info->final && info->index == 0 && info->len == len &&
                info->opcode == WS_TEXT) {
                // Complete text message received
                size_t copyLen = (len < GCODE_LINE_MAX_LENGTH) ? len : GCODE_LINE_MAX_LENGTH;
                memcpy(_wsRxBuffer, data, copyLen);
                _wsRxBuffer[copyLen] = '\0';
                _wsLineReady = true;
            }
            break;
        }

        case WS_EVT_ERROR:
            LOG_ERROR("WiFi: WebSocket error #%u: %s",
                      client->id(), (char*)data);
            break;

        default:
            break;
    }
}

// ============================================================================
// Constructor & Init
// ============================================================================

WifiComms::WifiComms() {
    strncpy(_ssid, WIFI_AP_SSID_PREFIX, sizeof(_ssid));
    strncpy(_password, WIFI_AP_PASSWORD, sizeof(_password));
}

void WifiComms::init() {
    // Initialize LittleFS for serving web UI
    if (!LittleFS.begin(true)) {
        LOG_ERROR("WiFi: LittleFS mount failed");
    }

    // Create server and WebSocket
    _server = new AsyncWebServer(WEBSOCKET_PORT);
    _ws = new AsyncWebSocket(WEBSOCKET_PATH);
    _ws->onEvent(onWsEvent);
    _server->addHandler(_ws);

    // ── Serve embedded web UI from LittleFS ─────────────────────────────
    _server->serveStatic("/", LittleFS, "/www/").setDefaultFile("index.html");

    // ── REST API endpoints ──────────────────────────────────────────────

    // GET /api/status — Machine status (JSON)
    _server->on("/api/status", HTTP_GET, [](AsyncWebServerRequest* request) {
        request->send(200, "application/json",
                      "{\"status\":\"ok\",\"firmware\":\"OpenPlotter\"}");
    });

    // GET /api/settings — All machine settings
    _server->on("/api/settings", HTTP_GET, [](AsyncWebServerRequest* request) {
        // TODO: Return actual settings as JSON
        request->send(200, "application/json", "{}");
    });

    // POST /api/gcode — Send a G-code command
    _server->on("/api/gcode", HTTP_POST,
                [](AsyncWebServerRequest* request) {},
                NULL,
                [](AsyncWebServerRequest* request, uint8_t* data,
                   size_t len, size_t index, size_t total) {
        size_t copyLen = (len < GCODE_LINE_MAX_LENGTH) ? len : GCODE_LINE_MAX_LENGTH;
        memcpy(_wsRxBuffer, data, copyLen);
        _wsRxBuffer[copyLen] = '\0';
        _wsLineReady = true;
        request->send(200, "text/plain", "ok");
    });

    // 404 handler
    _server->onNotFound([](AsyncWebServerRequest* request) {
        request->send(404, "text/plain", "Not found");
    });

    // ── Start WiFi ──────────────────────────────────────────────────────
    if (_mode == 0) {
        // AP Mode
        // Append last 4 chars of MAC to SSID for uniqueness
        uint8_t mac[6];
        WiFi.macAddress(mac);
        char apSSID[32];
        snprintf(apSSID, sizeof(apSSID), "%s-%02X%02X",
                 WIFI_AP_SSID_PREFIX, mac[4], mac[5]);

        WiFi.softAP(apSSID, _password, WIFI_AP_CHANNEL);
        LOG_INFO("WiFi: AP mode — SSID: %s  IP: %s",
                 apSSID, WiFi.softAPIP().toString().c_str());
    } else {
        // STA Mode
        WiFi.mode(WIFI_STA);
        WiFi.setHostname(WIFI_HOSTNAME);
        WiFi.begin(_ssid, _password);

        LOG_INFO("WiFi: Connecting to %s...", _ssid);
        uint32_t startTime = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - startTime < 15000) {
            delay(500);
        }

        if (WiFi.status() == WL_CONNECTED) {
            LOG_INFO("WiFi: Connected — IP: %s", WiFi.localIP().toString().c_str());
        } else {
            LOG_ERROR("WiFi: Connection failed, falling back to AP mode");
            // Fallback to AP mode
            WiFi.softAP(WIFI_AP_SSID_PREFIX, _password);
            _mode = 0;
        }
    }

    // Start mDNS
    if (MDNS.begin(WIFI_MDNS_NAME)) {
        MDNS.addService("http", "tcp", WEBSOCKET_PORT);
        LOG_INFO("WiFi: mDNS started — http://%s.local", WIFI_MDNS_NAME);
    }

    // Start web server
    _server->begin();
    LOG_INFO("WiFi: Web server started on port %d", WEBSOCKET_PORT);
}

// ============================================================================
// Main Loop Update
// ============================================================================

void WifiComms::update() {
    if (_ws) {
        _ws->cleanupClients();
    }
}

// ============================================================================
// Read Line
// ============================================================================

char* WifiComms::readLine() {
    if (_wsLineReady) {
        _wsLineReady = false;
        memcpy(_lineBuffer, _wsRxBuffer, sizeof(_lineBuffer));
        return _lineBuffer;
    }
    return nullptr;
}

// ============================================================================
// Broadcast
// ============================================================================

void WifiComms::broadcast(const char* msg) {
    if (_ws) {
        _ws->textAll(msg);
    }
}

void WifiComms::sendStatus(const char* state, float x, float y, float z,
                            float feedRate, bool toolOn) {
    if (!_ws || _ws->count() == 0) return;

    char buf[192];
    snprintf(buf, sizeof(buf),
             "{\"state\":\"%s\",\"pos\":{\"x\":%.3f,\"y\":%.3f,\"z\":%.3f},"
             "\"feed\":%.0f,\"tool\":%s}",
             state, x, y, z, feedRate, toolOn ? "true" : "false");
    _ws->textAll(buf);
}

// ============================================================================
// WiFi Status
// ============================================================================

bool WifiComms::isConnected() const {
    return (_mode == 0) || (WiFi.status() == WL_CONNECTED);
}

const char* WifiComms::getIP() const {
    static char ip[16];
    if (_mode == 0) {
        strncpy(ip, WiFi.softAPIP().toString().c_str(), sizeof(ip));
    } else {
        strncpy(ip, WiFi.localIP().toString().c_str(), sizeof(ip));
    }
    return ip;
}

const char* WifiComms::getSSID() const {
    return _ssid;
}

void WifiComms::setCredentials(const char* ssid, const char* password) {
    strncpy(_ssid, ssid, sizeof(_ssid) - 1);
    strncpy(_password, password, sizeof(_password) - 1);
}

void WifiComms::setMode(uint8_t mode) {
    _mode = mode;
}

#endif // !BRIDGE_MODE
#endif // HAS_WIFI
