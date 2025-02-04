// Header file includes
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>
#include <MD_Parola.h>
#include <SPI.h>
#include "Font_Data.h"
#include <WebServer.h>

#define HARDWARE_TYPE MD_MAX72XX::GENERIC_HW
#define MAX_DEVICES 2  // Changed to match 2x1 display

#define CLK_PIN   18 // or SCK
#define DATA_PIN  23 // or MOSI
#define CS_PIN    5  // or SS

MD_Parola P = MD_Parola(HARDWARE_TYPE, DATA_PIN, CLK_PIN, CS_PIN, MAX_DEVICES);

#define SPEED_TIME  65
#define PAUSE_TIME  0

/**********  WiFi & Weather/Time Config  ******************************/
char* ssid = "Lights";
char* password = "1234567@8";

// Default timezone offset in seconds (19800 = UTC+5:30)
int timezoneinSeconds = 19800;
int dst = 0;  // Daylight Saving Time offset (if needed)

// Default location for Madurai, Tamil Nadu, India
float latitude = 9.8773;
float longitude = 78.0795;

// We'll update this URL based on the current latitude and longitude
String weatherURL;

uint16_t h, m, s;
char szTime[32];  // Display: hh:mm:ss | xx°C
float temperature = 0.0;

WebServer server(80);

/***************** Utility Functions **************************/

// Update the weather URL based on the current latitude and longitude
void updateWeatherURL() {
  weatherURL = "https://api.open-meteo.com/v1/forecast?latitude=" + String(latitude, 4) +
               "&longitude=" + String(longitude, 4) + "&current_weather=true";
}

// Get current time and format it along with temperature
void getTime(char *psz) {
  time_t now = time(nullptr);
  struct tm* p_tm = localtime(&now);
  if (p_tm) {
    h = p_tm->tm_hour;
    m = p_tm->tm_min;
    s = p_tm->tm_sec;
    sprintf(psz, "%02d:%02d:%02d  %.0f" "\xB0" "C ", h, m, s, temperature);
    Serial.println(psz);
  }
}

// Get current weather data from Open-Meteo
void getWeather() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(weatherURL);
    int httpResponseCode = http.GET();
    
    if (httpResponseCode == 200) {
      String response = http.getString();
      // Allocate a JSON document (adjust capacity if needed)
      DynamicJsonDocument doc(1024);
      
      DeserializationError error = deserializeJson(doc, response);
      if (!error) {
        // Open-Meteo returns current weather data under "current_weather"
        temperature = doc["current_weather"]["temperature"].as<float>();
        Serial.print("Temperature: ");
        Serial.print(temperature);
        Serial.println("°C");
      } else {
        Serial.print("deserializeJson() failed: ");
        Serial.println(error.f_str());
      }
    } else {
      Serial.print("Error getting weather, HTTP code: ");
      Serial.println(httpResponseCode);
    }
    http.end();
  } else {
    Serial.println("WiFi not connected");
  }
}

// Get time from NTP servers using the set timezone
void getTimentp() {
  configTime(timezoneinSeconds, dst, "pool.ntp.org", "time.nist.gov");
  while (!time(nullptr)) {
    delay(500);
    Serial.print("Connecting to NTP...");
  }
  Serial.println(" Time Updated");
}

/***************** Web Server Handlers **************************/

// Serve the main page with a colorful HTML form for settings, including a drop-down for cities
void handleRoot() {
  String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'><title>ESP32 Weather & Time Config</title>";
  html += "<style>";
  html += "body { font-family: Arial, sans-serif; background: linear-gradient(to right, #00c6ff, #0072ff); color: #fff; text-align: center; }";
  html += "h1 { margin-top: 50px; }";
  html += "form { background: rgba(0, 0, 0, 0.5); padding: 20px; display: inline-block; border-radius: 10px; }";
  html += "input[type='text'], input[type='number'], select { padding: 5px; margin: 5px; border: none; border-radius: 5px; }";
  html += "input[type='submit'] { padding: 10px 20px; margin-top: 10px; border: none; border-radius: 5px; background: #ff9800; color: #fff; font-size: 16px; }";
  html += "</style>";
  
  // JavaScript to update the input fields when a city is selected from the drop-down list.
  html += "<script>";
  html += "function updateCity() {";
  html += "  var select = document.getElementById('citySelect');";
  html += "  var value = select.value;";
  html += "  if (value !== 'custom') {";
  html += "    var parts = value.split(',');"; // parts[0]=city, parts[1]=lat, parts[2]=lon, parts[3]=tz
  html += "    document.getElementById('lat').value = parts[1];";
  html += "    document.getElementById('lon').value = parts[2];";
  html += "    document.getElementById('tz').value = parts[3];";
  html += "  }";
  html += "}";
  html += "</script>";
  
  html += "</head><body>";
  html += "<h1>ESP32 Weather & Time Config</h1>";
  html += "<p>Current Time and Temperature: ";
  char buffer[64];
  getTime(buffer);
  html += String(buffer);
  html += "</p>";
  
  html += "<form action='/update' method='POST'>";
  
  // Drop-down list for selecting a city
  html += "<label>Select City:</label><br>";
  html += "<select id='citySelect' name='city' onchange='updateCity()'>";
  html += "<option value='custom'>Custom</option>";
  html += "<option value='New York,40.7128,-74.0060,-18000'>New York, USA (UTC-5)</option>";
  html += "<option value='Los Angeles,34.0522,-118.2437,-28800'>Los Angeles, USA (UTC-8)</option>";
  html += "<option value='London,51.5074,-0.1278,0'>London, UK (UTC+0)</option>";
  html += "<option value='Paris,48.8566,2.3522,3600'>Paris, France (UTC+1)</option>";
  html += "<option value='Moscow,55.7558,37.6173,10800'>Moscow, Russia (UTC+3)</option>";
  html += "<option value='Beijing,39.9042,116.4074,28800'>Beijing, China (UTC+8)</option>";
  html += "<option value='Tokyo,35.6895,139.6917,32400'>Tokyo, Japan (UTC+9)</option>";
  html += "<option value='Sydney,-33.8688,151.2093,36000'>Sydney, Australia (UTC+10)</option>";
  html += "<option value='Dubai,25.2048,55.2708,14400'>Dubai, UAE (UTC+4)</option>";
  html += "<option value='Singapore,1.3521,103.8198,28800'>Singapore (UTC+8)</option>";
  html += "<option value='Mumbai,19.0760,72.8777,19800'>Mumbai, India (UTC+5:30)</option>";
  html += "<option value='Delhi,28.7041,77.1025,19800'>Delhi, India (UTC+5:30)</option>";
  html += "<option value='Kolkata,22.5726,88.3639,19800'>Kolkata, India (UTC+5:30)</option>";
  html += "<option value='Chennai,13.0827,80.2707,19800'>Chennai, India (UTC+5:30)</option>";
  html += "<option value='Bengaluru,12.9716,77.5946,19800'>Bengaluru, India (UTC+5:30)</option>";
  html += "<option value='Berlin,52.5200,13.4050,3600'>Berlin, Germany (UTC+1)</option>";
  html += "<option value='Rome,41.9028,12.4964,3600'>Rome, Italy (UTC+1)</option>";
  html += "<option value='Madrid,40.4168,-3.7038,3600'>Madrid, Spain (UTC+1)</option>";
  html += "<option value='Cairo,30.0444,31.2357,7200'>Cairo, Egypt (UTC+2)</option>";
  html += "<option value='Johannesburg,-26.2041,28.0473,7200'>Johannesburg, South Africa (UTC+2)</option>";
  html += "<option value='Buenos Aires,-34.6037,-58.3816,-10800'>Buenos Aires, Argentina (UTC-3)</option>";
  html += "<option value='Rio de Janeiro,-22.9068,-43.1729,-10800'>Rio de Janeiro, Brazil (UTC-3)</option>";
  html += "<option value='Mexico City,19.4326,-99.1332,-21600'>Mexico City, Mexico (UTC-6)</option>";
  html += "<option value='Vancouver,49.2827,-123.1207,-28800'>Vancouver, Canada (UTC-8)</option>";
  html += "<option value='Toronto,43.6532,-79.3832,-18000'>Toronto, Canada (UTC-5)</option>";
  html += "</select><br><br>";
  
  // Input fields for latitude, longitude, and timezone offset
  html += "<label>Latitude:</label><br><input type='text' id='lat' name='lat' value='" + String(latitude, 4) + "'><br>";
  html += "<label>Longitude:</label><br><input type='text' id='lon' name='lon' value='" + String(longitude, 4) + "'><br>";
  html += "<label>Timezone Offset (in seconds):</label><br><input type='number' id='tz' name='tz' value='" + String(timezoneinSeconds) + "'><br>";
  html += "<input type='submit' value='Update Settings'>";
  html += "</form>";
  
  html += "</body></html>";
  
  server.send(200, "text/html", html);
}

// Handle form submission to update settings
void handleUpdate() {
  // If the form contains latitude, longitude, and timezone offset
  if (server.hasArg("lat") && server.hasArg("lon") && server.hasArg("tz")) {
    latitude = server.arg("lat").toFloat();
    longitude = server.arg("lon").toFloat();
    timezoneinSeconds = server.arg("tz").toInt();
    
    updateWeatherURL();
    getTimentp();  // update time with new timezone settings
    
    // Immediately update the weather after settings are changed
    getWeather();

    Serial.println("Settings updated:");
    Serial.print("Latitude: "); Serial.println(latitude);
    Serial.print("Longitude: "); Serial.println(longitude);
    Serial.print("Timezone (seconds): "); Serial.println(timezoneinSeconds);
  }
  // Redirect back to the main page
  server.sendHeader("Location", "/", true);
  server.send(302, "text/plain", "");
}

/***************** Setup and Loop **************************/
void setup() {
  Serial.begin(115200);
  delay(10);
  
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println("\nWiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  
  updateWeatherURL();  // Set initial weather URL
  
  delay(3000);
  WiFi.mode(WIFI_STA);
  getTimentp();
  
  P.begin();
  P.setInvert(false);
  P.setIntensity(5);
  P.displayClear();
  
  // Get the initial weather data and time
  getWeather();
  getTime(szTime);
  P.displayText(szTime, PA_CENTER, SPEED_TIME, PAUSE_TIME, PA_SCROLL_LEFT, PA_SCROLL_RIGHT);
  
  // Set up web server routes
  server.on("/", handleRoot);
  server.on("/update", HTTP_POST, handleUpdate);
  server.begin();
  Serial.println("Web server started.");
}

void loop() {
  server.handleClient();
  
  static unsigned long lastWeatherUpdate = 0;
  // Update weather every 5 minutes (300,000 milliseconds)
  if (millis() - lastWeatherUpdate > 300000) {
    getWeather();
    lastWeatherUpdate = millis();
  }
  
  if (P.displayAnimate()) {
    getTime(szTime);
    P.displayReset();
    P.displayText(szTime, PA_CENTER, SPEED_TIME, PAUSE_TIME, PA_SCROLL_LEFT, PA_SCROLL_DOWN);
  }
}
