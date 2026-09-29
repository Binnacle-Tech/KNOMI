#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "moonraker.h"
#include "knomi.h"
#include "octoprint_ws.h"
#include "knomi_ble.h"

// #define MOONRAKER_DEBUG

void lv_popup_warning(const char * warning, bool clickable);

// Percent-encode a path, keeping '/' so folder structure survives.
static String url_encode_path(const String &in) {
    const char *hex = "0123456789ABCDEF";
    String out;
    out.reserve(in.length() * 3);
    for (size_t i = 0; i < in.length(); i++) {
        uint8_t c = (uint8_t)in[i];
        if (isalnum(c) || c == '/' || c == '-' || c == '_' || c == '.' || c == '~') {
            out += (char)c;
        } else {
            out += '%';
            out += hex[c >> 4];
            out += hex[c & 0x0F];
        }
    }
    return out;
}

static void octoprint_popup_error(int code, const String &response) {
    static char msg[160];
    String text;
    if (!response.isEmpty()) {
        DynamicJsonDocument json_parse(response.length() * 2 + 256);
        if (deserializeJson(json_parse, response) == DeserializationError::Ok &&
            json_parse["error"].is<const char *>()) {
            text = json_parse["error"].as<String>();
        } else {
            text = response;
        }
    }
    if (code == 401 || code == 403) {
        text = "OctoPrint rejected the API key";
    }
    snprintf(msg, sizeof(msg), "OctoPrint error %d\n%s", code, text.c_str());
    lv_popup_warning(msg, true);
}

String MOONRAKER::send_request(const char * type, String path, String body) {
    String ip = knomi_config.moonraker_ip;
    String port = knomi_config.moonraker_port;
    String url = "http://" + ip + ":" + port + path;
    String response = "";
    bool octo = knomi_backend_is_octoprint();
    HTTPClient client;
    // replace all " " space to "%20" for http
    url.replace(" ", "%20");
    client.begin(url);
    if (octo) {
        if (knomi_config.api_key[0])
            client.addHeader("X-Api-Key", knomi_config.api_key);
        if (!body.isEmpty())
            client.addHeader("Content-Type", "application/json");
    }
    // set timeout to 60 seconds since some gcode like G28 need long time to feedback
    client.setTimeout(60000);
    int code = client.sendRequest(type, body);
    last_code = code;
    // http request success
    if (code > 0) {
        unconnected = false;
        response = client.getString();
        if (octo) {
            auth_failed = (code == 401 || code == 403);
            // Only surface errors for user actions; polling handles 409 etc. itself
            if (strcmp(type, "POST") == 0 && code >= 400) {
                octoprint_popup_error(code, response);
            }
        } else if (code == 400) {
            if (!response.isEmpty()) {
                // Serial.println(response.c_str());
                DynamicJsonDocument json_parse(response.length() * 2);
                deserializeJson(json_parse, response);
                String msg = json_parse["error"]["message"].as<String>();
#ifdef MOONRAKER_DEBUG
                Serial.println(msg.c_str());
#endif
                msg.remove(0, 41); //  remove header {'error': 'WebRequestError', 'message':
                msg.remove(msg.length() - 2, 2); // remove tail }
                msg.replace("\\n", "\n");
                lv_popup_warning(msg.c_str(), true);
            }
        }
    } else {
        /*
         * since some gcode need long time cause code=-11 error
         * so don't set status when POST gcode
         * only set when GET
         */
        if (strcmp(type, "GET") == 0)
            unconnected = true;
        Serial.printf("%s http %s error.\r\n", octo ? "octoprint" : "moonraker", type);
    }
    client.end(); //Free the resources

#ifdef MOONRAKER_DEBUG
    Serial.printf("\r\n\r\n %s code:%d************ %s *******************\r\n\r\n", type, code, url.c_str());
    Serial.println(response.c_str());
    Serial.println("\r\n*******************************\r\n\r\n");
#endif

    return response;
}

void MOONRAKER::http_post_loop(void) {
    if (post_queue.count == 0) return;
    if (knomi_ble_link_active())
        knomi_ble_send_command(post_queue.queue[post_queue.index_r]); // plugin translates
    else if (knomi_backend_is_octoprint())
        octoprint_post(post_queue.queue[post_queue.index_r]);
    else
        send_request("POST", post_queue.queue[post_queue.index_r]);
    post_queue.count--;
    post_queue.index_r = (post_queue.index_r + 1) % QUEUE_LEN;
}

bool MOONRAKER::post_to_queue(String path) {
    if (post_queue.count >= QUEUE_LEN) {
        Serial.println("moonraker post queue overflow!");
        return false;
    }
    post_queue.queue[post_queue.index_w] = path;
    post_queue.index_w = (post_queue.index_w + 1) % QUEUE_LEN;
    post_queue.count++;
#ifdef MOONRAKER_DEBUG
    Serial.printf("\r\n\r\n ************ post queue *******************\r\n\r\n");
    Serial.print("count: ");   Serial.println(post_queue.count);
    Serial.print("index_w: "); Serial.println(post_queue.index_w);
    Serial.print("queue: ");   Serial.println(path);
    Serial.println("\r\n*******************************\r\n\r\n");
#endif
    return true;
}

bool MOONRAKER::post_gcode_to_queue(String gcode) {
    String path = "/printer/gcode/script?script=" + gcode;
    return post_to_queue(path);
}

void MOONRAKER::get_printer_ready(void) {
    String webhooks = send_request("GET", "/printer/objects/query?webhooks");
    if (!webhooks.isEmpty()) {
        DynamicJsonDocument json_parse(webhooks.length() * 2);
        deserializeJson(json_parse, webhooks);
        String state = json_parse["result"]["status"]["webhooks"]["state"].as<String>();
        unready = (state == "ready") ? false : true;
#ifdef MOONRAKER_DEBUG
        Serial.print("unready: ");
        Serial.println(unready);
#endif
    } else {
        unready = true;
        Serial.println("Empty: moonraker: get_printer_ready");
    }
}

void MOONRAKER::get_printer_info(void) {
    String printer_info = send_request("GET", "/api/printer");
    if (!printer_info.isEmpty()) {
        DynamicJsonDocument json_parse(printer_info.length() * 2);
        deserializeJson(json_parse, printer_info);
        data.pause = json_parse["state"]["flags"]["pausing"].as<bool>(); // pausing
        data.pause |= json_parse["state"]["flags"]["paused"].as<bool>(); // paused
        data.printing = json_parse["state"]["flags"]["printing"].as<bool>(); // printing
        data.printing |= json_parse["state"]["flags"]["cancelling"].as<bool>(); // cancelling
        data.printing |= data.pause; // pause
        data.bed_actual = int16_t(json_parse["temperature"]["bed"]["actual"].as<double>() + 0.5f);
        data.bed_target = int16_t(json_parse["temperature"]["bed"]["target"].as<double>() + 0.5f);
        data.nozzle_actual = int16_t(json_parse["temperature"][knomi_config.moonraker_tool]["actual"].as<double>() + 0.5f);
        data.nozzle_target = int16_t(json_parse["temperature"][knomi_config.moonraker_tool]["target"].as<double>() + 0.5f);
#ifdef MOONRAKER_DEBUG
        Serial.print("unoperational: ");
        Serial.println(unoperational);
        Serial.print("printing: ");
        Serial.println(data.printing);
        Serial.print("bed_actual: ");
        Serial.println(data.bed_actual);
        Serial.print("bed_target: ");
        Serial.println(data.bed_target);
        Serial.print("nozzle_actual: ");
        Serial.println(data.nozzle_actual);
        Serial.print("nozzle_target: ");
        Serial.println(data.nozzle_target);
#endif
    } else {
        Serial.println("Empty: moonraker: get_printer_info");
    }
}

// only return gcode file name except path
// for example:"SD:/test/123.gcode"
// only return "123.gcode"
const char * path_only_gcode(const char * path)
{
  char * name = strrchr(path, '/');

  if (name != NULL)
    return (name + 1);
  else
    return path;
}

void MOONRAKER::get_progress(void) {
    String display_status = send_request("GET", "/printer/objects/query?virtual_sdcard&print_stats&gcode_move=gcode_position");
    if (!display_status.isEmpty()) {
        DynamicJsonDocument json_parse(display_status.length() * 2);
        deserializeJson(json_parse, display_status);
        data.progress = (uint8_t)(json_parse["result"]["status"]["virtual_sdcard"]["progress"].as<double>() * 100 + 0.5f);
        String path = json_parse["result"]["status"]["virtual_sdcard"]["file_path"].as<String>();
        strlcpy(data.file_path, path_only_gcode(path.c_str()), sizeof(data.file_path) - 1);
        data.file_path[sizeof(data.file_path) - 1] = 0;
        // printing screen extras
        JsonVariant ps = json_parse["result"]["status"]["print_stats"];
        double duration = ps["print_duration"] | 0.0;
        double p = json_parse["result"]["status"]["virtual_sdcard"]["progress"] | 0.0;
        data.print_time = (uint32_t)duration;
        data.time_left = (p > 0.02 && duration > 60) ? (int32_t)(duration * (1.0 - p) / p) : -1;
        data.layer = ps["info"]["current_layer"] | 0;
        data.layer_total = ps["info"]["total_layer"] | 0;
        JsonVariant pos = json_parse["result"]["status"]["gcode_move"]["gcode_position"];
        data.z_um = pos.isNull() ? INT32_MIN : (int32_t)((pos[2] | 0.0) * 1000);
#ifdef MOONRAKER_DEBUG
        Serial.print("progress: ");
        Serial.println(data.progress);
        Serial.print("path: ");
        Serial.println(data.file_path);
#endif
    } else {
        Serial.println("Empty: moonraker: get_progress");
    }
}

void MOONRAKER::get_knomi_status(void) {
    String knomi_status = send_request("GET", "/printer/objects/query?gcode_macro%20_KNOMI_STATUS");
    if (!knomi_status.isEmpty()) {
        DynamicJsonDocument json_parse(knomi_status.length() * 2);
        deserializeJson(json_parse, knomi_status);
        data.homing = json_parse["result"]["status"]["gcode_macro _KNOMI_STATUS"]["homing"].as<bool>();
        data.probing = json_parse["result"]["status"]["gcode_macro _KNOMI_STATUS"]["probing"].as<bool>();
        data.qgling = json_parse["result"]["status"]["gcode_macro _KNOMI_STATUS"]["qgling"].as<bool>();
        data.heating_nozzle = json_parse["result"]["status"]["gcode_macro _KNOMI_STATUS"]["heating_nozzle"].as<bool>();
        data.heating_bed = json_parse["result"]["status"]["gcode_macro _KNOMI_STATUS"]["heating_bed"].as<bool>();
        // optional extra flags; false when not defined in _KNOMI_STATUS
        data.shaping = json_parse["result"]["status"]["gcode_macro _KNOMI_STATUS"]["shaping"].as<bool>();
        data.pid_tuning = json_parse["result"]["status"]["gcode_macro _KNOMI_STATUS"]["pid_tuning"].as<bool>();
        data.cleaning = json_parse["result"]["status"]["gcode_macro _KNOMI_STATUS"]["cleaning"].as<bool>();
        data.filament = json_parse["result"]["status"]["gcode_macro _KNOMI_STATUS"]["filament"].as<bool>();
        data.paused_ext = json_parse["result"]["status"]["gcode_macro _KNOMI_STATUS"]["paused"].as<bool>();
#ifdef MOONRAKER_DEBUG
        Serial.print("homing: ");
        Serial.println(data.homing);
        Serial.print("probing: ");
        Serial.println(data.probing);
        Serial.print("qgling: ");
        Serial.println(data.qgling);
        Serial.print("heating_nozzle: ");
        Serial.println(data.heating_nozzle);
        Serial.print("heating_bed: ");
        Serial.println(data.heating_bed);
#endif
    } else {
        Serial.println("Empty: moonraker: get_knomi_status");
    }
}

void MOONRAKER::http_get_loop(void) {
    if (knomi_backend_is_octoprint()) {
        octoprint_get_loop();
        return;
    }
    auth_failed = false;
    data_unlock = false;
    get_printer_ready();
    if (!unready) {
        // get_knomi_status() must before get_printer_info()
        // avoid homing, qgling, etc action flag = 1
        // but printing flag has not refresh
        get_knomi_status();
        get_printer_info();
        if (data.printing) {
            get_progress();
        }
    }
    data_unlock = true;
}

/* ---------------------------------------------------------------------------
 * OctoPrint backend
 *
 * The UI keeps queueing Moonraker-style paths (/printer/gcode/script?..., etc).
 * octoprint_post() translates them to the OctoPrint REST API, so the UI code
 * doesn't need to know which backend is active.
 * ------------------------------------------------------------------------- */

#define GCODE_SCRIPT_PREFIX "/printer/gcode/script?script="
#define PRINT_START_PREFIX  "/printer/print/start?filename="
#define SERVICE_PREFIX      "/machine/services/"

void MOONRAKER::octoprint_send_gcode(String gcode) {
    DynamicJsonDocument doc(gcode.length() + 128);
    doc["commands"][0] = gcode;
    String body;
    serializeJson(doc, body);
    send_request("POST", "/api/printer/command", body);
}

void MOONRAKER::octoprint_post(String path) {
    if (path.startsWith(GCODE_SCRIPT_PREFIX)) {
        octoprint_send_gcode(path.substring(strlen(GCODE_SCRIPT_PREFIX)));
    } else if (path.startsWith(PRINT_START_PREFIX)) {
        String file = path.substring(strlen(PRINT_START_PREFIX));
        send_request("POST", "/api/files/local/" + url_encode_path(file),
                     "{\"command\":\"select\",\"print\":true}");
    } else if (path == "/printer/print/cancel") {
        send_request("POST", "/api/job", "{\"command\":\"cancel\"}");
    } else if (path == "/printer/print/pause") {
        send_request("POST", "/api/job", "{\"command\":\"pause\",\"action\":\"pause\"}");
    } else if (path == "/printer/print/resume") {
        if (!data.pause && data.paused_ext) {
            // paused by Klipper (M600, MMU...) without OctoPrint knowing: resume in Klipper
            octoprint_send_gcode("RESUME");
        } else {
            send_request("POST", "/api/job", "{\"command\":\"pause\",\"action\":\"resume\"}");
        }
    } else if (path == "/printer/restart") {
        octoprint_send_gcode("RESTART");          // Klipper host restart (via OctoKlipper)
    } else if (path == "/printer/firmware_restart") {
        octoprint_send_gcode("FIRMWARE_RESTART");
    } else if (path == "/machine/reboot") {
        send_request("POST", "/api/system/commands/core/reboot", "{}");
    } else if (path == "/machine/shutdown") {
        send_request("POST", "/api/system/commands/core/shutdown", "{}");
    } else if (path.startsWith(SERVICE_PREFIX "restart")) {
        send_request("POST", "/api/system/commands/core/restart", "{}");
    } else {
        lv_popup_warning("Not supported\nwith OctoPrint", true);
    }
}

void MOONRAKER::octoprint_parse_printer(const String &printer_info) {
    DynamicJsonDocument json_parse(printer_info.length() * 2 + 256);
    if (deserializeJson(json_parse, printer_info) != DeserializationError::Ok) {
        Serial.println("octoprint: bad /api/printer json");
        return;
    }
    JsonObject flags = json_parse["state"]["flags"];
    data.pause = flags["pausing"].as<bool>() | flags["paused"].as<bool>();
    data.printing = flags["printing"].as<bool>() | flags["cancelling"].as<bool>()
                  | flags["resuming"].as<bool>() | flags["finishing"].as<bool>();
    data.printing |= data.pause;
    data.bed_actual = int16_t(json_parse["temperature"]["bed"]["actual"].as<double>() + 0.5f);
    data.bed_target = int16_t(json_parse["temperature"]["bed"]["target"].as<double>() + 0.5f);
    data.nozzle_actual = int16_t(json_parse["temperature"][knomi_config.moonraker_tool]["actual"].as<double>() + 0.5f);
    data.nozzle_target = int16_t(json_parse["temperature"][knomi_config.moonraker_tool]["target"].as<double>() + 0.5f);
}

// Optional companion plugin (OctoPrint-KNOMI) reports homing/probing/QGL/heating.
// Without it, heating is still detected from temperatures; homing/probing/QGL
// animations just won't show.
void MOONRAKER::octoprint_get_knomi_status(void) {
    if (octo_plugin_retry_ms && (int32_t)(millis() - octo_plugin_retry_ms) < 0) {
        return;
    }
    String status = send_request("GET", "/api/plugin/knomi");
    if (last_code == 200 && !status.isEmpty()) {
        octo_plugin_retry_ms = 0;
        DynamicJsonDocument json_parse(status.length() * 2 + 128);
        if (deserializeJson(json_parse, status) == DeserializationError::Ok) {
            data.homing = json_parse["homing"].as<bool>();
            data.probing = json_parse["probing"].as<bool>();
            data.qgling = json_parse["qgling"].as<bool>();
            data.heating_nozzle = json_parse["heating_nozzle"].as<bool>();
            data.heating_bed = json_parse["heating_bed"].as<bool>();
            data.shaping = json_parse["shaping"].as<bool>();
            data.pid_tuning = json_parse["pid_tuning"].as<bool>();
            data.cleaning = json_parse["cleaning"].as<bool>();
            data.filament = json_parse["filament"].as<bool>();
            data.paused_ext = json_parse["paused"].as<bool>();
            data.runout = json_parse["runout"] | false;
            data.fan = json_parse["fan"] | 0;
            data.speed = json_parse["speed"] | 0;
            JsonVariant tp = json_parse["time_progress"];
            data.progress_mode = tp.isNull() ? 0 : (tp.as<bool>() ? 2 : 1);
        }
    } else {
        data.homing = data.probing = data.qgling = false;
        data.heating_nozzle = data.heating_bed = false;
        data.shaping = data.pid_tuning = data.cleaning = data.filament = false;
        data.paused_ext = data.runout = false;
        data.fan = 0; data.speed = 0;
        if (last_code == 404) data.progress_mode = 0;
        if (last_code == 404) {
            // plugin not installed, don't hammer OctoPrint with 404s
            octo_plugin_retry_ms = millis() + 30000;
        }
    }
}

// Progress as OctoPrint's dashboard shows it. With PrintTimeGenius the bar is
// elapsed / (elapsed + remaining) whenever a time-left estimate exists;
// otherwise it is the file position ("completion"). The plugin reports whether
// PrintTimeGenius is enabled; without the plugin, guess from the estimate's origin.
uint8_t octo_progress(JsonVariantConst p) {
    double pct = p["completion"] | 0.0;
    double left = p["printTimeLeft"] | 0.0;
    uint8_t mode = moonraker.data.progress_mode;
    const char *origin = p["printTimeLeftOrigin"] | "";
    bool time_based = mode == 2 || (mode == 0 && strcmp(origin, "genius") == 0);
    if (left > 0 && time_based) {
        double t = p["printTime"] | 0.0;
        pct = t / (t + left) * 100.0;
    }
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    return (uint8_t)(pct + 0.5);
}

void MOONRAKER::octoprint_get_progress(void) {
    String job = send_request("GET", "/api/job");
    if (last_code != 200 || job.isEmpty()) return;
    DynamicJsonDocument json_parse(job.length() * 2 + 256);
    if (deserializeJson(json_parse, job) != DeserializationError::Ok) return;
    data.progress = octo_progress(json_parse["progress"]);
    String name = json_parse["job"]["file"]["name"] | "";
    strlcpy(data.file_path, path_only_gcode(name.c_str()), sizeof(data.file_path));
    data.print_time = json_parse["progress"]["printTime"] | 0;
    JsonVariant left = json_parse["progress"]["printTimeLeft"];
    data.time_left = left.isNull() ? -1 : left.as<int32_t>();
    // no layer info from OctoPrint itself; Z comes over the websocket
    data.layer = data.layer_total = 0;
}

void MOONRAKER::octoprint_get_loop(void) {
    data_unlock = false;
    String printer_info = send_request("GET", "/api/printer?exclude=sd");
    if (last_code == 200 && !printer_info.isEmpty()) {
        unready = false;
        octoprint_get_knomi_status();
        octoprint_parse_printer(printer_info);
        if (data.printing) {
            octoprint_get_progress();
        }
    } else {
        // 409: OctoPrint up but printer not connected/operational
        // 401/403: bad API key (auth_failed set in send_request)
        unready = true;
    }
    data_unlock = true;
}

// Newline separated list of printable files for the "print" roller
static void octoprint_collect_files(JsonArrayConst files, String &out) {
    for (JsonObjectConst f : files) {
        const char *type = f["type"] | "";
        if (strcmp(type, "folder") == 0) {
            octoprint_collect_files(f["children"].as<JsonArrayConst>(), out);
        } else if (strcmp(type, "machinecode") == 0) {
            out += f["path"].as<const char *>();
            out += "\n";
        }
    }
}

bool MOONRAKER::get_file_list(String &out) {
    out = "";
    if (knomi_ble_link_active()) {
        return knomi_ble_file_list(out);
    }
    if (!knomi_backend_is_octoprint()) {
        String list = send_request("GET", "/server/files/list?");
        if (list.isEmpty()) return false;
        DynamicJsonDocument json_parse(list.length() * 2);
        deserializeJson(json_parse, list);
        JsonArray files = json_parse["result"].as<JsonArray>();
        for (JsonObject file : files) {
            out += file["path"].as<String>() + "\n";
        }
        return true;
    }

    // OctoPrint's listing includes analysis/history per file and can be large,
    // so stream it through a filter instead of buffering the whole body.
    String url = "http://" + String(knomi_config.moonraker_ip) + ":" +
                 String(knomi_config.moonraker_port) + "/api/files/local?recursive=true";
    HTTPClient client;
    client.useHTTP10(true); // no chunked encoding -> parse straight from the stream
    client.begin(url);
    if (knomi_config.api_key[0])
        client.addHeader("X-Api-Key", knomi_config.api_key);
    client.setTimeout(15000);
    int code = client.GET();
    last_code = code;
    bool ok = false;
    if (code == 200) {
        DynamicJsonDocument filter(1024);
        JsonObject lvl = filter["files"].createNestedObject();
        for (uint8_t depth = 0; depth < 6; depth++) {
            lvl["path"] = true;
            lvl["type"] = true;
            if (depth < 5) lvl = lvl["children"].createNestedObject();
        }
        DynamicJsonDocument doc(64 * 1024); // lands in PSRAM
        DeserializationError err = deserializeJson(doc, client.getStream(),
                                                   DeserializationOption::Filter(filter));
        if (err == DeserializationError::Ok || err == DeserializationError::NoMemory) {
            octoprint_collect_files(doc["files"].as<JsonArrayConst>(), out);
            ok = true;
        } else {
            Serial.printf("octoprint file list json error: %s\r\n", err.c_str());
        }
    } else if (code > 0) {
        String response = client.getString();
        octoprint_popup_error(code, response);
    }
    client.end();
    return ok;
}

MOONRAKER moonraker;

void moonraker_post_task(void * parameter) {
    for(;;) {
        moonraker.http_post_loop();
        delay(500);
    }
}

void moonraker_task(void * parameter) {
    moonraker.data.time_left = -1;
    moonraker.data.z_um = INT32_MIN;

    xTaskCreate(moonraker_post_task, "moonraker post",
        4096,  // Stack size (bytes)
        NULL,  // Parameter to pass
        8,     // Task priority
        NULL   // Task handle
        );

    uint32_t next_poll = 0;
    for(;;) {
        // Bluetooth link: the plugin pushes status, nothing to poll
        knomi_ble_process();
        if (knomi_ble_link_active()) {
            octoprint_ws_loop(); // closes the websocket if it was open
            delay(20);
            continue;
        }
        if (wifi_get_connect_status() != WIFI_STATUS_CONNECTED) {
            delay(200);
            continue;
        }
        if (!knomi_backend_is_octoprint()) {
            octoprint_ws_loop(); // closes the socket if it was open
            moonraker.http_get_loop();
            delay(200);
            continue;
        }
        // OctoPrint: websocket pushes state ~2x/s. Poll over HTTP only while the
        // socket is down, plus a slow resync (plugin flags, file name, etc.)
        octoprint_ws_loop();
        bool healthy = octoprint_ws_healthy();
        if ((int32_t)(millis() - next_poll) >= 0) {
            moonraker.http_get_loop();
            next_poll = millis() + (healthy ? 10000 : 200);
        }
        delay(healthy ? 10 : 50);
    }
}

// Klipper Control: Restart Firmware Restart. /printer/restart, /printer/firmware_restart
// Service Control: stop start restart. POST /machine/services/stop|restart|start?service={name}
// Host Control: Reboot, Shutdown. POST /machine/shutdown, POST /machine/reboot
