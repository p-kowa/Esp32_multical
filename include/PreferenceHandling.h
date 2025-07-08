#include <Arduino.h>
#include <Preferences.h>

void setPreferences(
    const unsigned long& mqttPublishInterval,
    const String& mqtt_server,
    const String& mqtt_topic,
    const String& mqtt_user,
    const String& mqtt_pass,
    const String& mqtt_client
);

void getPreferences(
    unsigned long& mqttPublishInterval,
    String& mqtt_server,
    String& mqtt_topic,
    String& mqtt_user,
    String& mqtt_pass,
    String& mqtt_client
);
