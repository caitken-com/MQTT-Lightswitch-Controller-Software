#include <Wire.h>
#include <Ethernet.h>
#include <PubSubClient.h>
#include <I2CButton.h>
#include <Adafruit_MCP23017.h>



/* NETWORK SETTINGS */
#define ENABLE_MAC_ADDRESS_ROM			true
#define MAC_I2C_ADDR					0x50
byte network_mac[] = {0x0F, 0x51, 0xAF, 0x24, 0x3E, 0x4E};
EthernetClient network_client;


/* MQTT SETTINGS */
IPAddress mqtt_broker(XXX, XXX, XXX, XXX);
PubSubClient mqtt(network_client);

String mqtt_client = "Lcontrol-01";
const char mqtt_user[] = "XXXX";
const char mqtt_pass[] = "XXXX";
unsigned long mqtt_timer = 0;


/* Watchdog */
int watchdog_pin = 3;
bool watchdog_status = false;
unsigned long watchdog_timmer = 0;


/* Buttons */
Adafruit_MCP23017 mcps[6];
I2CButton buttons[96];



/**
 * Initial setup
 *
 * @return void
 */
void setup()
{
	Wire.begin();

	if (ENABLE_MAC_ADDRESS_ROM == true)
	{
		network_mac[0] = readRegister(0xFA);
		network_mac[1] = readRegister(0xFB);
		network_mac[2] = readRegister(0xFC);
		network_mac[3] = readRegister(0xFD);
		network_mac[4] = readRegister(0xFE);
		network_mac[5] = readRegister(0xFF);
	}

	// Wait for network connection
	while (Ethernet.begin(network_mac) == 0)
	{
		delay(1000);
	}

	// Initializes the pseudo-random number generator
	randomSeed(micros());

	// Configure MQTT
	mqtt.setServer(mqtt_broker, 1883);
	mqtt.setCallback(mqttCallback);

	// Setup pins and configure button
	int id = 0;
	for (int i = 0; i < 6; i++)
	{
		mcps[i].begin(i);

		for (int p = 0; p < 16; p ++)
		{
			mcps[i].pinMode(p, INPUT);
			buttons[id].configure(id, PULL_UP, onPress, onDblPress, onHold);
			id ++;
		}
	}
}


/**
 * Main program loop
 *
 * @return void
 */
void loop()
{
	// Keep network connection
	Ethernet.maintain();

	// Reconnect to MQTT server
	if (!mqtt.connected())
	{
		if (!mqttReconnect())
		{
			// Exit early of loop
			return;
		}
	}

	// Check for incoming MQTT messages
	mqtt.loop();

	// Update each button's state
	int id = 0;
	for (int i = 0; i < 6; i++)
	{
		for (int p = 0; p < 16; p ++)
		{
			buttons[id].check(mcps[i].digitalRead(p));
			id ++;
		}
	}

	// Prevent arduino from resetting if everything is fine
	resetWatchDog();
}



/**
 * MQTT reconnect
 * @return bool True on successful connection
 * @return bool False on failure to connect
 */
bool mqttReconnect()
{
	// Watchdog
	digitalWrite(watchdog_pin, LOW);
	
	// Too early to reconnect
	if (!getTimer(mqtt_timer, 1000))
	{
		return false;
	}

	// Subscribe if connected
	if (mqtt.connect(String(mqtt_client + String(random(0xffff), HEX)).c_str(), mqtt_user, mqtt_pass))
	{
		return true;
	}

	// Connection failed
	return false;
}



/**
* Press event call-back function
*
* @param int pin Reports which I/O pin the event occurred on
* @return void
*/
void onPress(int pin)
{
	char numberstring[2];
	sprintf(numberstring, "%d", pin);
	mqtt.publish("server/button/press", numberstring);
}



/**
* Double press event call-back function
*
* @param int pin Reports which I/O pin the event occurred on
* @return void
*/
void onDblPress(int pin)
{
	char numberstring[2];
	sprintf(numberstring, "%d", pin);
	mqtt.publish("server/button/dblpress", numberstring);
}



/**
* Press-hold event call-back function
*
* @param int pin Reports which I/O pin the event occurred on
* @return void
*/
void onHold(int pin)
{
	char numberstring[2];
	sprintf(numberstring, "%d", pin);
	mqtt.publish("server/button/holding", numberstring);
}



/**
 * Process incoming MQTT message
 *
 * @param char* topic The incomnig topic
 * @param byte* payload The incoming message
 * @param int leng Length of payload
 * @return void
 */
void mqttCallback(char* topic, byte* payload, unsigned int leng)
{
}



/**
 * Reset watch-dog module
 * Prevents Arduino from resetting if everything is fine
 *
 * @return void
 */
void resetWatchDog()
{
	if (getTimer(watchdog_timmer, 1000))
	{
		watchdog_status = !watchdog_status;
		digitalWrite(watchdog_pin, watchdog_status);
	}
}




/**
 * Reads byte value from given register
 *
 * @param byte reg The register
 * @return byte The value
 */
byte readRegister (byte reg)
{
	Wire.beginTransmission(MAC_I2C_ADDR);
	
	// Register to read
	Wire.write(reg);
	Wire.endTransmission();

	// Read a byte
	Wire.requestFrom(MAC_I2C_ADDR, 1);
	while(!Wire.available())
	{
		// Wait
	}

	return Wire.read();
}



/**
 * Determine if given timer has reached given interval
 *
 * @param unsigned long tmr The current timer
 * @param int interval Length of time to run timer
 * @return bool True when timer is complete
 * @return bool False when timer is counting
 */
bool getTimer (unsigned long &tmr, int interval)
{
	// Set initial value
	if (tmr < 1)
	{
		tmr = millis();
	}

	// Determine difference of our timer against millis()
	if (millis() - tmr >= interval)
	{
		// Complete. Reset timer
		tmr = 0;
		return true;
	}

	// Still needs to "count"
	return false;
}