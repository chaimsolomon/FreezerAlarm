#include <Ticker.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#define MQTT_KEEPALIVE 5

#include <OneWire.h>
#include <DallasTemperature.h>

#define ONE_WIRE_BUS D5
#define TEMPERATURE_PRECISION 12
#define ALARM_PIN D3

// Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);

// Pass our oneWire reference to Dallas Temperature.
DallasTemperature sensors(&oneWire);

// arrays to hold device addresses
DeviceAddress Thermometer;

volatile float temp = 50.0;

const char* ssid = "";
const char* password = "";
const char* mqtt_server = "10.100.102.170";

WiFiClient espClient;
PubSubClient client(espClient);
long lastMsg = 0;
char msg[50];
int value = 0;


Ticker SensorTicker;

volatile int sensor_state = 0;

void onConnectionEstablished();

void setup_wifi() {

  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  randomSeed(micros());

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

// MQTT
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  char* ptopic;
  ptopic = (char*) malloc(length + 1);
  strncpy(ptopic, (char*)payload, length);
  ptopic[length] = '\0';
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  free(ptopic);
}


void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "BigFreezer";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (client.connect(clientId.c_str())) {
      Serial.println("connected");
      // ... and resubscribe
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(1000);
    }
  }
}


void sensor_isr() {
  char out_str[50];
  switch (sensor_state) {
    case 0:
      Serial.println("Trigger Get Temp 0");
      sensors.setWaitForConversion(false);  // makes it async
      sensors.requestTemperatures();
      sensors.setWaitForConversion(true);
      digitalWrite(ALARM_PIN, LOW);
      break;      
    case 1:
      Serial.println("Get Temp 0");
      temp = sensors.getTempC(Thermometer);
      if (temp > -5) digitalWrite(ALARM_PIN, HIGH);

      sprintf(out_str, "%.0f", temp);
      Serial.println(out_str);
      client.publish("BigFreezer/temp", out_str);

      break;      
    default:
      sensor_state = -1;
  }
  sensor_state ++;
}


// function to print a device address
void printAddress(DeviceAddress deviceAddress)
{
  for (uint8_t i = 0; i < 8; i++)
  {
    // zero pad the address if necessary
    if (deviceAddress[i] < 16) Serial.print("0");
    Serial.print(deviceAddress[i], HEX);
  }
}

// function to print the temperature for a device
void printTemperature(DeviceAddress deviceAddress)
{
  float tempC = sensors.getTempC(deviceAddress);
  Serial.print("Temp C: ");
  Serial.print(tempC);
  Serial.print(" Temp F: ");
  Serial.print(DallasTemperature::toFahrenheit(tempC));
}

// function to print a device's resolution
void printResolution(DeviceAddress deviceAddress)
{
  Serial.print("Resolution: ");
  Serial.print(sensors.getResolution(deviceAddress));
  Serial.println();
}

// main function to print information about a device
void printData(DeviceAddress deviceAddress)
{
  Serial.print("Device Address: ");
  printAddress(deviceAddress);
  Serial.print(" ");
  printTemperature(deviceAddress);
  Serial.println();
}

void setup()
{
  pinMode(LED_BUILTIN, OUTPUT);     // Initialize the LED_BUILTIN pin as an output
  pinMode(ALARM_PIN, OUTPUT);
  digitalWrite(ALARM_PIN, LOW);
  Serial.begin(115200);
  SensorTicker.attach_ms(500, sensor_isr);
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  sensors.begin();
  // locate devices on the bus
  Serial.print("Locating devices...");
  Serial.print("Found ");
  Serial.print(sensors.getDeviceCount(), DEC);
  Serial.println(" devices.");
  // report parasite power requirements
  Serial.print("Parasite power is: ");
  if (sensors.isParasitePowerMode()) Serial.println("ON");
  else Serial.println("OFF");
  if (!sensors.getAddress(Thermometer, 0)) Serial.println("Unable to find address for Device 0");
  // show the addresses we found on the bus
  Serial.print("Device 0 Address: ");
  printAddress(Thermometer);
  Serial.println();

  // set the resolution 
  sensors.setResolution(Thermometer, TEMPERATURE_PRECISION);

  Serial.print("Device 0 Resolution: ");
  Serial.print(sensors.getResolution(Thermometer), DEC);
  Serial.println();

  sensors.setWaitForConversion(false);
}

void loop()
{
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
}
