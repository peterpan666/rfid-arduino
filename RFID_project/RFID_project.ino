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
#define BUFFER_LENGHT  20

#define BIP_TASK_PERIOD 500
#define BIP_DURATION 100
#define BIP_IDLE   0
#define BIP_START  1
#define BIP_STOP   2
#define BUZZER_PIN 6

#define DEBUG true
#define debug_println(...)   if (DEBUG) Serial.println(__VA_ARGS__)
#define debug_print(...)     if (DEBUG) Serial.print(__VA_ARGS__)

unsigned long last_sw_task;
unsigned char v1 = HIGH, v2 = LOW;

unsigned char bip_state = BIP_IDLE;
unsigned long last_bip_task = 0;
unsigned long refresh_bip_task = BIP_TASK_PERIOD;

unsigned long last_write_task = 0;
unsigned long refresh_write_task = 300;
unsigned int tag_detected = 1;

unsigned long temp = 0;

unsigned long last_connection_task = 0;
unsigned long refresh_connection_task = 5000;

unsigned long last_reading_task = 0;
unsigned long refresh_reading_task = 1000;

// Sequence to send to the reader to make a complete inventory
byte inventory[] = { 0x02, 0x00, 0x08, 0x00, 0x00, 0x00, 0x08, 0x00, 0x01, 0xAA, 0x55, 0x00, 0x00, 0x28, 0x68 };
char data_buffer[BUFFER_LENGHT];

String send_buffer[32];
String Id;
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
    char buffer[30] = {0};
    int i = 0;
    
    while (send_write != send_read) {
      
      send_buffer[send_read++].toCharArray(buffer,12);
      data += String(buffer);
      if(send_read >= 31)
            send_read = 0;
      data += ',';
      
      if (++i >= 10) {
        break;
      }
    }
    debug_println(data);
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

void init_reading_task()
{
  debug_println("Initialisation");
  delay(500);
  Serial.flush(); 
}

void main_reading_task(void)
{
    if (millis() < last_reading_task + refresh_reading_task) 
    {
      return;
    }
    
    last_reading_task = millis();
    Serial.flush();
    Serial.write(inventory, sizeof(inventory)); //send the command to start new inventory  
}

void main_write_task(void)
{
    int i, NbTags = 0;
    
    if (millis() < last_write_task + refresh_write_task) {
      return;
    }
    
    last_write_task = millis();
    //tag_detected++;
    if(Serial.available())
    {
      
      Serial.readBytes(data_buffer, 7);
      if(data_buffer[0] == 0x02)
      {
        Serial.readBytes(data_buffer, 3);
        NbTags = data_buffer[2];
        tag_detected += NbTags;
        //tag_detected += 3;
        
        for(i=0; i<NbTags; i++)
        {
          //digitalWrite(3, 1);
          Serial.readBytes(data_buffer, 1);    //EPClen

          Serial.readBytes(data_buffer, 12);   //EPC
          
          for(unsigned int j=0; j<12; j++)
          {
            send_buffer[send_write] += data_buffer[j];
          }
          send_write++;
          
          if(send_write >= 31)
            send_write = 0;
            
          Serial.readBytes(data_buffer, 3);    // AntID + Nbread
          //digitalWrite(3, 0);
        } 
      }
     Serial.flush(); 
    } 
}

void init_sw_task() {
  last_sw_task = 0;
  pinMode(SW_V1, OUTPUT);
  pinMode(SW_V2, OUTPUT);
  digitalWrite(SW_V1, v1);
  digitalWrite(SW_V2, v2);
}

void main_sw_task() {
  if (millis() < (last_sw_task + SW_PERIOD))
    return;
    
  last_sw_task = millis();
  v1 = v1 ? LOW : HIGH;
  v2 = v2 ? LOW : HIGH;
  digitalWrite(SW_V1, v1);
  digitalWrite(SW_V2, v2);
} 

void init_bip_task(){
  pinMode(BUZZER_PIN, OUTPUT);
}

void main_bip_task() {
  if (millis() < (last_bip_task + refresh_bip_task))
    return;
    
  last_bip_task = millis();
  
  switch(bip_state){
    case BIP_IDLE :
      if (tag_detected > 0)
        bip_state = BIP_START;
      break;

    case BIP_START :
      if (tag_detected != 0) {
        analogWrite(BUZZER_PIN, 800);
        refresh_bip_task = BIP_DURATION;
        bip_state = BIP_STOP;
      } else {
        bip_state = BIP_IDLE;
        refresh_bip_task = BIP_TASK_PERIOD;
      }
      break;
      
    case BIP_STOP :
      analogWrite(BUZZER_PIN, 0);
      bip_state = BIP_START;
      refresh_bip_task = BIP_TASK_PERIOD;
      tag_detected--;
      break;
  }    
  return;
}

void setup() {
  Serial.begin(115200);
  init_connection_task(); 
  //init_sw_task();
  init_bip_task();
  init_reading_task();
  delay(1000);
}

void loop() {
  main_connection_task();
  //main_sw_task();
  main_bip_task();
  main_reading_task();
  main_write_task();
}


