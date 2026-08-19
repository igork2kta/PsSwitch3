#include "templates.h"
#include <stdio.h>

#define SSDP_RESPONSE \
    "HTTP/1.1 200 OK\r\n" \
    "EXT:\r\n" \
    "CACHE-CONTROL: max-age=100\r\n" \
    "LOCATION: http://%s:80/description.xml\r\n" \
    "SERVER: FreeRTOS/6.0.5, UPnP/1.0, IpBridge/1.17.0\r\n" \
    "hue-bridgeid: 22024D0686D5\r\n" \
    "ST: urn:schemas-upnp-org:device:basic:1\r\n" \
    "USN: uuid:2f402f80-da50-11e1-9b23-22024D0686D5::upnp:rootdevice\r\n" \
    "\r\n"

int template_build_ssdp_response(char *out, size_t len, const char *ip)
{
    return snprintf(out, len, SSDP_RESPONSE, ip);
}

#define DESCRIPTION_XML_RESPONSE "<?xml version=\"1.0\"?>\n" \
        "<root xmlns=\"urn:schemas-upnp-org:device-1-0\">\n" \
        "<specVersion><major>1</major><minor>0</minor></specVersion>\n" \
        "<URLBase>http://%s:80/</URLBase>\n" \
        "<device>\n" \
        "<deviceType>urn:schemas-upnp-org:device:Basic:1</deviceType>\n" \
        "<friendlyName>FauxmoC#</friendlyName>\n" \
        "<manufacturer>Royal Philips Electronics</manufacturer>\n" \
        "<manufacturerURL>http://www.philips.com</manufacturerURL>\n" \
        "<modelDescription>Philips hue Personal Wireless Lighting</modelDescription>\n" \
        "<modelName>Philips hue bridge 2012</modelName>\n" \
        "<modelNumber>929000226503</modelNumber>\n" \
        "<modelURL>http://www.meethue.com</modelURL>\n" \
        "<modelid>BSB001</modelid>\n" \
        "<bridgeid>22024D0686D5</bridgeid>\n" \
        "<mac>%s</mac>\n" \
        "<serialNumber>22024D0686D5</serialNumber>\n" \
        "<UDN>uuid:2f402f80-da50-11e1-9b23-22024D0686D5</UDN>\n" \
        "<presentationURL>index.html</presentationURL>\n" \
        "</device>\n" \
        "</root>\n" 

int template_build_description_xml(char *out, size_t out_len, const char *ip, const char *mac){
    return snprintf(out, out_len, DESCRIPTION_XML_RESPONSE, ip, mac);
}

#define LIGHTS_TEMPLATE_SHORT \
        "{"\
            "\"1\":{" \
                "\"type\":\"Extended color light\"," \
                "\"psrtype\":\"%s\"," \
                "\"name\":\"%s\"," \
                "\"uniqueid\":\"%s\"" \
            "}" \
        "}"

int template_build_lights_short(char *out, size_t out_len, const char *name, const char *type, const char *unique_id){
    return snprintf(out, out_len, LIGHTS_TEMPLATE_SHORT, type, name, unique_id);
}

#define LIGHT_TEMPLATE_LONG "{\n" \
    "  \"type\": \"Extended color light\",\n" \
    "  \"psrtype\": \"%s\",\n" \
    "  \"name\": \"%s\",\n" \
    "  \"uniqueid\": \"%s\",\n" \
    "  \"modelid\": \"LCT015\",\n" \
    "  \"manufacturername\": \"Philips\",\n" \
    "  \"productname\": \"E4\",\n" \
    "  \"state\": {\n" \
    "    \"on\": %s,\n" \
    "    \"bri\": %u,\n" \
    "    \"timerState\": %s,\n" \
    "    \"minutes\": %d,\n" \
    "    \"startState\": %s,\n" \
    "    \"otaState\": %s,\n" \
    "    \"temperature\": %.1f,\n" \
    "    \"thermalShutdownCount\": %u,\n" \
    "    \"wifiSsid\": \"%s\",\n" \
    "    \"wifiSignal\": %d,\n" \
    "    \"sat\": 0,\n" \
    "    \"effect\": \"none\",\n" \
    "    \"colormode\": \"xy\",\n" \
    "    \"ct\": 500,\n" \
    "    \"mode\": \"homeautomation\",\n" \
    "    \"reachable\": true\n" \
    "  },\n" \
    "  \"capabilities\": {\n" \
    "    \"certified\": false,\n" \
    "    \"streaming\": {\n" \
    "      \"renderer\": true,\n" \
    "      \"proxy\": false\n" \
    "    }\n" \
    "  },\n" \
    "  \"swversion\": \"%s\"\n" \
    "}"

int template_build_light_long(char *out, size_t out_len, const char *name, const char *type, const char *unique_id, const uint8_t bri, const bool state, const bool timer_state, int minutes, 
                                const bool start_state, const bool ota_state,  const float temperature, const uint8_t thermal_shutdown_count, const char *wifi_ssid, const int8_t wifi_signal, const char *sw_version){
    return snprintf(out, out_len, LIGHT_TEMPLATE_LONG, 
        type, 
        name, 
        unique_id, 
        (state) ? "true" : "false", 
        (unsigned int)(bri), 
        (timer_state) ? "true" : "false", 
        minutes,
        (start_state) ? "true" : "false",
        (ota_state) ? "true" : "false",
        temperature,
        thermal_shutdown_count,
        wifi_ssid,
        wifi_signal,
        sw_version
    );
}

#define LIGHT_STATE_SUCCESS_TEMPLATE \
    "[" \
        "{\"success\":{\"/lights/1/state/on\":%s}}," \
        "{\"success\":{\"/lights/1/state/bri\":%u}}" \
    "]"

int template_build_light_state_success(char *out, size_t out_len, bool state, uint8_t bri){
    return snprintf(out, out_len,
             LIGHT_STATE_SUCCESS_TEMPLATE,
             state ? "true" : "false",
             (unsigned int)bri);
}
#define LIGHT_TIMER_SUCCESS_TEMPLATE \
    "[" \
        "{\"success\":{\"/lights/1/timer/on\":%s}}," \
        "{\"success\":{\"/lights/1/timer/minutes\":%d}}" \
    "]"

int template_build_light_timer_success(char *out,
                                       size_t out_len,
                                       bool timer_state,
                                       uint16_t minutes)
{
    return snprintf(out, out_len,
             LIGHT_TIMER_SUCCESS_TEMPLATE,
             timer_state ? "true" : "false",
             minutes);
}


#define REQUEST_MESSAGE_TEMPLATE "{\"message\": \"%s\"}"


int template_build_message(char *out, size_t out_len, const char *message)
{
    return snprintf(out, out_len,
             REQUEST_MESSAGE_TEMPLATE,
             message);
}