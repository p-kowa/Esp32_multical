#include "PreferenceHandling.h"

Preferences preferences;

void setPreferences(
    const unsigned long& mqttPublishInterval,
    const String& mqtt_server,
    const String& mqtt_topic,
    const String& mqtt_user,
    const String& mqtt_pass,
    const String& mqtt_client
)
{
  preferences.begin("multical21", false); // false = read-write mode
  // Store the interval directly as an unsigned long in milliseconds for type safety and efficiency.
  preferences.putULong("interval", mqttPublishInterval);
  preferences.putString("mqtt_server", mqtt_server);
  preferences.putString("mqtt_topic", mqtt_topic);
  preferences.putString("mqtt_user", mqtt_user);
  preferences.putString("mqtt_pass", mqtt_pass);
  preferences.putString("mqtt_client", mqtt_client);
  preferences.end();
}

void getPreferences(
    unsigned long& mqttPublishInterval,
    String& mqtt_server,
    String& mqtt_topic,
    String& mqtt_user,
    String& mqtt_pass,
    String& mqtt_client
)
{
    preferences.begin("multical21", true);

    // Retrieve the interval in milliseconds, with a default of 96 seconds (96000 ms).
    mqttPublishInterval = preferences.getULong("interval", 96000);

    // Retrieve string settings with their respective defaults.
    mqtt_server  = preferences.getString("mqtt_server", "");
    mqtt_topic   = preferences.getString("mqtt_topic", "multical21");
    mqtt_user    = preferences.getString("mqtt_user", "");
    mqtt_pass    = preferences.getString("mqtt_pass", "");
    mqtt_client  = preferences.getString("mqtt_client", "Multical_");
    preferences.end();
}