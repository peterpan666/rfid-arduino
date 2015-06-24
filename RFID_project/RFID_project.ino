/*
Projet de RFID
Polytech' Montpellier
Contributeurs: Adrien Peyrouty
              Bastien Boissin
              Elie Faes
*/
#include <SPI.h>
#include <Ethernet.h>

#define SW_V1 8
#define SW_V2 9
#define SW_PERIOD 100

#define DEBUG false
#define debug_println(...)   if (DEBUG) Serial.println(__VA_ARGS__)
#define debug_print(...)     if (DEBUG) Serial.print(__VA_ARGS__)

unsigned long sw_millis;
unsigned char v1 = HIGH, v2 = LOW;

unsigned long last_write_task = 0;
unsigned long temp = 0;

unsigned long last_connection_task = 0;
unsigned long refresh_connection_task = 3000;

unsigned int send_buffer[256] = {0};
byte send_write = 0;
byte send_read = 0;

String room = "SE2";

// Enter a MAC address for your controller below.
// Newer Ethernet shields have a MAC address printed on a sticker on the shield
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
// if you don't want to use DNS (and reduce your sketch size)
// use the numeric IP instead of the name for the server:
//IPAddress server(74,125,232,128);  // numeric IP for Google (no DNS)
char server[] = "elie.weblogin.fr";    // name address for Google (using DNS)

// Set the static IP address to use if the DHCP fails to assign
IPAddress ip(192,168,0,177);

// Initialize the Ethernet client library
// with the IP address and port of the server 
// that you want to connect to (port 80 is default for HTTP):
EthernetClient client;

boolean ethernet_init()
{
  debug_print("Ethernet init... ");
  if (Ethernet.begin(mac) == 0) {
    debug_println("Failed to configure Ethernet using DHCP");
    // no point in carrying on, so do nothing forevermore:
    // try to congifure using IP address instead of DHCP:
    //Ethernet.begin(mac, ip)
    //return true;
    return false;
  } else {
    debug_println("DHCP OK");
    return true;
  }
}

void main_write_task(void)
{
    if (millis() < last_write_task + 4000) {
      return;
    }
    
    last_write_task = millis();
    
    send_buffer[send_write++] = temp++;
}

void init_connection_task() {
  while (!ethernet_init());
}

void main_connection_task(void)
{
    
    if (millis() < last_connection_task + refresh_connection_task) {
      return;
    }
    
    last_connection_task = millis();
    
    String data;
    
    int i = 0;
    
    while (send_write != send_read) {
      data += String(send_buffer[send_read++]) + ',';
      if (++i >= 10) {
        break;
      }
    }
    
    data = data.substring(0, data.length() - 1);
    
    if (data.length() == 0) {
      return;
    }
    
    String data_start = "{\"room\":\"" + room + "\",\"tag_id\":[";
    
    debug_println();
    debug_println(data);
    debug_println();
    
    debug_println("connecting...");
    
    if (client.connect(server, 80)) {
      debug_println("connected");
      debug_println("sending POST request");
      // Make a HTTP request:
      client.println("POST /classroom/store HTTP/1.0");
      client.print("Host: ");
      client.println(server);
      client.println("Content-type: application/json");
      client.println("Content-length: " + String(data_start.length() + data.length() + 2));
      client.println();
      client.print(data_start);
      client.print(data);
      client.print("]}");
      
      debug_println();
      debug_println("disconnecting.");
      client.stop();
    } 
    else {
      // kf you didn't get a connection to the server:
      debug_println("connection failed");
      return;
    }
}

void init_sw_task() {
  sw_millis = 0;
  pinMode(SW_V1, OUTPUT);
  pinMode(SW_V2, OUTPUT);
  digitalWrite(SW_V1, v1);
  digitalWrite(SW_V2, v2);
}

void main_sw_task() {
  if (millis() < (sw_millis + SW_PERIOD))
    return;
  
  sw_millis = millis();
  v1 = v1 ? LOW : HIGH;
  v2 = v2 ? LOW : HIGH;
  digitalWrite(SW_V1, v1);
  digitalWrite(SW_V2, v2);
}

void setup() {
  init_connection_task(); 
  init_sw_task();
  delay(1000);
}

void loop() {
  main_write_task();
  main_connection_task();
  main_sw_task();
}


