//#include <Android.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <DHT.h>
#include <DHT_U.h>
#include <Adafruit_SGP30.h>

#include "config.h"
#include "version.h"

#define EXPLODE4(arr) (arr[0], arr[1], arr[2], arr[3])

enum LogLevel {
    DEBUG,
    INFO,
    ERROR,
};

void setup_dht_sensor();
void setup_sgp_sensor();
void setup_wifi();
void setup_http_server();
void handle_http_root();
void handle_http_metrics();
void handle_http_not_found();
void log_request();
void read_humidity_sensor();
void read_temperature_sensor();
void read_heat_index();
void read_sgp();
void handle_http_home_client();
void handle_http_metrics_client();
void read_sensors(boolean force=false);
bool read_sensor(float (*function)(), float *value);
void log(char const *message, LogLevel level=LogLevel::INFO);
void get_http_method_name(char *name, size_t name_length, HTTPMethod method);

DHT dht_sensor(DHT_PIN, DHT_TYPE);
Adafruit_SGP30 sgp_sensor;

ESP8266WebServer http_server(HTTP_SERVER_PORT);

char sgp30_serial[13];
float humidity, temperature, heat_index;
uint32_t sgp30_counter = 0;
uint16_t tvoc, co2, h2, ethanol;
uint32_t previous_read_time = 0;

void setup(void) {
    char message[128];

    Serial.begin(9600);
    
    setup_dht_sensor();
    setup_sgp_sensor();

    // Test all the sensors
    read_sensors(true);

    setup_wifi();
    
    setup_http_server();
    
    snprintf(message, 128, "Prometheus namespace: %s", PROM_NAMESPACE);
    log(message);
    log("Setup done");
}

void setup_dht_sensor() {
    log("Setting up DHT sensor...");
    dht_sensor.begin();
    delay(1000);
    log("DHT sensor ready", LogLevel::DEBUG);
}

void setup_sgp_sensor() {
    log("Setting up SGP30 sensor...");
    if (!sgp_sensor.begin()) {
        log("SGP30 sensor failed to begin!", LogLevel::ERROR);
        log("Failed to setup SGP30 sensor!", LogLevel::ERROR);
        return;
    }

    snprintf(sgp30_serial, 13, "%x%x%x", sgp_sensor.serialnumber[0], sgp_sensor.serialnumber[1], sgp_sensor.serialnumber[2]);
    char message[32];
    snprintf(message, 32, "SGP30 serial #%s", sgp30_serial);

    sgp_sensor.softReset();
}

void setup_wifi() {
    char message[128];
    log("Setting up Wi-Fi");
    snprintf(message, 128, "Wi-Fi SSID: %s", WIFI_SSID);
    log(message, LogLevel::DEBUG);
    snprintf(message, 128, "MAC address: %s", WiFi.macAddress().c_str());
    log(message, LogLevel::DEBUG);
    snprintf(message, 128, "Initial hostname: %s", WiFi.hostname().c_str());
    log(message, LogLevel::DEBUG);

    WiFi.mode(WIFI_STA);

    #if WIFI_IPV4_STATIC == true
        log("Using static IPv4 adressing");
        IPAddress static_address(WIFI_IPV4_ADDRESS);
        IPAddress static_subnet(WIFI_IPV4_SUBNET_MASK);
        IPAddress static_gateway(WIFI_IPV4_GATEWAY);
        IPAddress static_dns1(WIFI_IPV4_DNS_1);
        IPAddress static_dns2(WIFI_IPV4_DNS_2);
        if (!WiFi.config(static_address, static_gateway, static_subnet, static_dns1, static_dns2)) {
            log("Failed to configure static addressing", LogLevel::ERROR);
        }
    #endif

    #ifdef WIFI_HOSTNAME
        log("Requesting hostname: " WIFI_HOSTNAME);
        if (WiFi.hostname(WIFI_HOSTNAME)) {
            log("Hostname changed");
        } else {
            log("Failed to change hostname (too long?)", LogLevel::ERROR);
        }
    #endif

    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED) {
        log("Wi-Fi connection not ready, waiting", LogLevel::DEBUG);
        delay(500);
    }

    log("Wi-Fi connected.");
    snprintf(message, 128, "SSID: %s", WiFi.SSID().c_str());
    log(message);
    snprintf(message, 128, "BSSID: %s", WiFi.BSSIDstr().c_str());
    log(message);
    snprintf(message, 128, "Hostname: %s", WiFi.hostname().c_str());
    log(message);
    snprintf(message, 128, "MAC address: %s", WiFi.macAddress().c_str());
    log(message);
    snprintf(message, 128, "IPv4 address: %s", WiFi.localIP().toString().c_str());
    log(message);
    snprintf(message, 128, "IPv4 subnet mask: %s", WiFi.subnetMask().toString().c_str());
    log(message);
    snprintf(message, 128, "IPv4 gateway: %s", WiFi.gatewayIP().toString().c_str());
    log(message);
    snprintf(message, 128, "Primary DNS server: %s", WiFi.dnsIP(0).toString().c_str());
    log(message);
    snprintf(message, 128, "Secondary DNS server: %s", WiFi.dnsIP(1).toString().c_str());
    log(message);
}

void setup_http_server() {
    char message[128];
    log("Setting up HTTP server");
    http_server.on("/", HTTPMethod::HTTP_GET, handle_http_root);
    http_server.on(HTTP_METRICS_ENDPOINT, HTTPMethod::HTTP_GET, handle_http_metrics);
    http_server.onNotFound(handle_http_not_found);
    http_server.begin();
    log("HTTP server started", LogLevel::DEBUG);
    snprintf(message, 128, "Metrics endpoint: %s", HTTP_METRICS_ENDPOINT);
    log(message);
}

void loop(void) {
    http_server.handleClient();
}

void handle_http_root() {
    log_request();
    static size_t const BUFSIZE = 256;
    static char const *response_template =
        "Prometheus ESP8266 DHT Exporter by HON95.\n"
        "\n"
        "Project: https://github.com/HON95/prometheus-esp8266-dht-exporter\n"
        "\n"
        "Usage: %s\n";
    char response[BUFSIZE];
    snprintf(response, BUFSIZE, response_template, HTTP_METRICS_ENDPOINT);
    http_server.send(200, "text/plain; charset=utf-8", response);
}

void handle_http_metrics() {
    log_request();
    static size_t const BUFSIZE = 2000;
    static char const *response_template =
        "# HELP " PROM_NAMESPACE "_info Metadata about the device.\n"
        "# TYPE " PROM_NAMESPACE "_info gauge\n"
        "# UNIT " PROM_NAMESPACE "_info \n"
        PROM_NAMESPACE "_info{version=\"%s\",board=\"%s\"} 1\n"
        "# HELP " PROM_NAMESPACE "_info Metadata about a sensor.\n"
        "# TYPE " PROM_NAMESPACE "_info gauge\n"
        "# UNIT " PROM_NAMESPACE "_info \n"
        PROM_NAMESPACE "_sensor{sensor=\"%s\",serial=\"%s\"} 1\n"
        PROM_NAMESPACE "_sensor{sensor=\"%s\",serial=\"%s\"} 1\n"
        "# HELP " PROM_NAMESPACE "_air_humidity_percent Air humidity.\n"
        "# TYPE " PROM_NAMESPACE "_air_humidity_percent gauge\n"
        "# UNIT " PROM_NAMESPACE "_air_humidity_percent %%\n"
        PROM_NAMESPACE "_air_humidity_percent %f\n"
        "# HELP " PROM_NAMESPACE "_air_temperature_celsius Air temperature.\n"
        "# TYPE " PROM_NAMESPACE "_air_temperature_celsius gauge\n"
        "# UNIT " PROM_NAMESPACE "_air_temperature_celsius \u00B0C\n"
        PROM_NAMESPACE "_air_temperature_celsius %f\n"
        "# HELP " PROM_NAMESPACE "_air_heat_index_celsius Apparent air temperature, based on temperature and humidity.\n"
        "# TYPE " PROM_NAMESPACE "_air_heat_index_celsius gauge\n"
        "# UNIT " PROM_NAMESPACE "_air_heat_index_celsius \u00B0C\n"
        PROM_NAMESPACE "_air_heat_index_celsius %f\n"
        "# HELP " PROM_NAMESPACE "_air_quality_eco2 Equivalent calculated carbon-dioxide (eCO2).\n"
        "# TYPE " PROM_NAMESPACE "_air_quality_eco2 gauge\n"
        "# UNIT " PROM_NAMESPACE "_air_quality_eco2 ppm\n"
        PROM_NAMESPACE "_air_quality_eco2 %d\n"
        "# HELP " PROM_NAMESPACE "_air_quality_tvoc Total Volatile Organic Compound (TVOC).\n"
        "# TYPE " PROM_NAMESPACE "_air_quality_tvoc gauge\n"
        "# UNIT " PROM_NAMESPACE "_air_quality_tvoc ppb\\t\n"
        PROM_NAMESPACE "_air_quality_tvoc %d\n"
        "# HELP " PROM_NAMESPACE "_air_quality_h2 Hydrogen.\n"
        "# TYPE " PROM_NAMESPACE "_air_quality_h2 gauge\n"
        "# UNIT " PROM_NAMESPACE "_air_quality_h2 ppm\n"
        PROM_NAMESPACE "_air_quality_h2 %d\n"
        "# HELP " PROM_NAMESPACE "_air_quality_ethanol Ethanol.\n"
        "# TYPE " PROM_NAMESPACE "_air_quality_ethanol gauge\n"
        "# UNIT " PROM_NAMESPACE "_air_quality_ethanol ppm\n"
        PROM_NAMESPACE "_air_quality_ethanol %d\n";

    read_sensors();
    if (isnan(humidity) || isnan(temperature) || isnan(heat_index) || isnan(tvoc) || isnan(co2) || isnan(h2) || isnan(ethanol)) {
        http_server.send(500, "text/plain; charset=utf-8", "Sensor error.");
        return;
    }

    char response[BUFSIZE];
    snprintf(response, BUFSIZE, response_template, VERSION, BOARD_NAME, DHT_NAME, "n/a", "SGP30", sgp30_serial, humidity, temperature, heat_index, tvoc, co2, h2, ethanol);
    http_server.send(200, "text/plain; charset=utf-8", response);
}

void handle_http_not_found() {
    log_request();
    http_server.send(404, "text/plain; charset=utf-8", "Not found.");
}

void read_sensors(boolean force) {
    uint32_t min_interval = max(READ_INTERVAL, 1000);
    uint32_t current_time = millis();

    if (!force && current_time - previous_read_time < min_interval) {
        log("Sensors were recently read, will not read again yet.", LogLevel::DEBUG);
        return;
    }
    previous_read_time = current_time;

    read_humidity_sensor();
    read_temperature_sensor();
    read_heat_index();
    read_sgp();
}

void read_humidity_sensor() {
    log("Reading humidity sensor ...", LogLevel::DEBUG);

    read_sensor([] {
        return dht_sensor.readHumidity();
    }, &humidity);

    if (humidity) {
        humidity += HUMIDITY_CORRECTION_OFFSET;
    } else {
        log("Failed to read humidity sensor.", LogLevel::ERROR);
    }
}

void read_temperature_sensor() {
    log("Reading temperature sensor ...", LogLevel::DEBUG);
    
    read_sensor([] {
        return dht_sensor.readTemperature();
    }, &temperature);

    if (temperature) {
        temperature += TEMPERATURE_CORRECTION_OFFSET;
    } else {
        log("Failed to read temperature sensor.", LogLevel::ERROR);
    }
}

void read_heat_index() {
    if (!isnan(humidity) && !isnan(temperature)) {
        heat_index = dht_sensor.computeHeatIndex(temperature, humidity, false); // compute in celsius
    } else {
        heat_index = NAN;
    }
}

float getAbsoluteHumidity(float t, float h) {
    float absoluteHumidity = (2.167 * 6.112) * h ;
    absoluteHumidity *= exp((17.62 * t)/(243.12 + t));
    absoluteHumidity /=  (273.15 + t);

    return absoluteHumidity;
}

void read_sgp() {
    log("Reading air quality (SGP30) sensor ...", LogLevel::DEBUG);

    sgp_sensor.setHumidity(getAbsoluteHumidity(temperature, humidity));

    if (sgp_sensor.IAQmeasure() && sgp_sensor.IAQmeasureRaw()) {
        tvoc = sgp_sensor.TVOC;
        co2 = sgp_sensor.eCO2;
        h2 = sgp_sensor.rawH2;
        ethanol = sgp_sensor.rawEthanol;

        sgp30_counter++;
        if (sgp30_counter == 30) {
            sgp30_counter = 0;

            uint16_t tvoc_base, eco2_base;
            if (!sgp_sensor.getIAQBaseline(&eco2_base, &tvoc_base)) {
                log("Failed to get baseline readings", LogLevel::ERROR);
                return;
            }

            char message[128];
            snprintf(message, 128, "Baseline values: eCO2: 0x%s & TVOC: 0x%s", String(eco2_base, HEX).c_str(), String(tvoc_base, HEX).c_str());
            log(message, LogLevel::DEBUG);
        }
    } else {
        log("Failed to read air quality sensor.", LogLevel::ERROR);
    }
}

bool read_sensor(float (*function)(), float *value) {
    bool success = false;
    for (int i = 0; i < READ_TRY_COUNT; i++) {
        *value = function();
        if (!isnan(*value)) {
            success = true;
            break;
        }
        log("Failed to read sensor.", LogLevel::DEBUG);
    }
    return success;
}

void log_request() {
    char message[128];
    char method_name[16];
    get_http_method_name(method_name, 16, http_server.method());
    snprintf(message, 128, "Request: client=%s:%u method=%s path=%s",
            http_server.client().remoteIP().toString().c_str(), http_server.client().remotePort(), method_name, http_server.uri().c_str());
    log(message, LogLevel::INFO);
}

void get_http_method_name(char *name, size_t name_length, HTTPMethod method) {
    switch (method) {
    case HTTP_GET:
        snprintf(name, name_length, "GET");
        break;
    case HTTP_HEAD:
        snprintf(name, name_length, "HEAD");
        break;
    case HTTP_POST:
        snprintf(name, name_length, "POST");
        break;
    case HTTP_PUT:
        snprintf(name, name_length, "PUT");
        break;
    case HTTP_PATCH:
        snprintf(name, name_length, "PATCH");
        break;
    case HTTP_DELETE:
        snprintf(name, name_length, "DELETE");
        break;
    case HTTP_OPTIONS:
        snprintf(name, name_length, "OPTIONS");
        break;
    default:
        snprintf(name, name_length, "UNKNOWN");
        break;
    }
}

void log(char const *message, LogLevel level) {
    if (DEBUG_MODE == 0 && level == LogLevel::DEBUG) {
        return;
    }
    // Will overflow after a while
    float seconds = millis() / 1000.0;
    char str_level[10];
    switch (level) {
        case DEBUG:
            strcpy(str_level, "DEBUG");
            break;
        case INFO:
            strcpy(str_level, "INFO");
            break;
        case ERROR:
            strcpy(str_level, "ERROR");
            break;
        default:
            break;
    }
    char record[150];
    snprintf(record, 150, "[%10.3f] [%-5s] %s", seconds, str_level, message);
    Serial.println(record);
}
