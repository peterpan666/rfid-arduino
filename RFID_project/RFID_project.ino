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
#define BUFFER_LENGHT  255

#define BIP_TASK_PERIOD 100
#define BIP_DURATION 100
#define BIP_IDLE   0
#define BIP_START  1
#define BIP_STOP   2
#define BUZZER_PIN 6

#define DEBUG false
#define debug_println(...)   if (DEBUG) Serial.println(__VA_ARGS__)
#define debug_print(...)     if (DEBUG) Serial.print(__VA_ARGS__)

unsigned long last_sw_task;
unsigned char v1 = HIGH, v2 = LOW;

unsigned char bip_state = BIP_IDLE;
unsigned long last_bip_task = 0;
unsigned long refresh_bip_task = BIP_TASK_PERIOD;

unsigned long last_write_task = 0;
unsigned long refresh_write_task = 200;
unsigned int tag_detected = 1;

unsigned long temp = 0;

unsigned long last_connection_task = 0;
unsigned long refresh_connection_task = 5000;

unsigned long last_reading_task = 0;
unsigned long refresh_reading_task = 1000;

// Sequence to send to the reader to make a complete inventory
byte inventory[] = { 0x02,0x00,0x08,0x00,0x00,0x00,0x08,0x00,0x01,0xAA,0x55,0x00,0x00,0x28,0x68 };

char send_buffer[32][12];

byte send_write = 0;
byte send_read = 0;
boolean Flag_request = false;
boolean Flag_response = true;
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

void htoa(char h[12], char s[24]) 
{
 char low, high;
 for (int i = 0; i < 12; i++) 
 {
    low = h[i] & 0x0f;
    high = (h[i] >> 4) & 0x0f;
    s[i*2 + 1] = low < 10 ? low + 48 : low + 65 - 10;
    s[i*2] = high < 10 ? high + 48 : high + 65 - 10;
 }
}

void main_connection_task(void)
{
    
    if (millis() < last_connection_task + refresh_connection_task) {
      return;
    }
    
    last_connection_task = millis();
    
    String data;
    char buffer[24] = {0};
    int i = 0;
    
    while (send_write != send_read) {
      
      data += "\"";
      htoa(send_buffer[send_read], buffer);
      
      for(unsigned int j=0; j<24; j++)
      {
        data += String(buffer[j]);
      }
      data += "\",";
      
      send_read++;
      if(send_read >= 31)
            send_read = 0;

      if (++i >= 10) {
        break;
      }
    }
    //debug_println(data);
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
  debug_println("Initialisation reading");
  delay(500);
  Serial.flush(); 
}

void main_reading_task(void)
{
    if (millis() < last_reading_task + refresh_reading_task /*&& !Flag_response*/) 
    {
      return;
    }
    debug_print("start reading ...");
    last_reading_task = millis();
  
    Serial1.write(inventory, sizeof(inventory)); //send the command to start new inventory
    debug_println(" End reading"); 
    //Flag_request = true; 
    //Flag_response = false;
}

void my_memcpy(char* desti, char* source, int Nbyte, int offset)
{
  for(int i =0; i< Nbyte; i++)
  {
    desti[i] = source[i + offset];
  }  
}

void main_write_task(void)
{
    unsigned int i, NbTags = 0;
    char vide = 0;
    char header_buffer[BUFFER_LENGHT] = {0};
    char data_buffer[BUFFER_LENGHT] = {0};
    
    if (millis() < last_write_task + refresh_write_task /*&& !Flag_request*/) {
      return;
    }
    debug_print("start writing...");
    last_write_task = millis();
    
    if(Serial1.available())
    {
      //Flag_response = true;
      
      Serial1.readBytes(header_buffer, 9);
      if(header_buffer[0] == 0x02 && header_buffer[6] == 0x01)
      {
        unsigned int Lin = header_buffer[7]*256 + header_buffer[8];
        debug_print("Lin = ");
        debug_println(Lin, DEC);
        for(unsigned int j=0; j<9; j++)
        {
          debug_print(header_buffer[j], HEX);
          debug_print(" ");
        }
        if(Lin < 17)
        {
         Flag_request = false;
         return;
        }
         
        Serial1.readBytes(data_buffer, Lin);
        
        NbTags = data_buffer[0];
        tag_detected += NbTags;
        debug_print("\n\r Nombre de tags = ");
        debug_println(NbTags);
        
        for(i=0; i<NbTags; i++)
        {
          my_memcpy(send_buffer[send_write++], data_buffer, 12, 2 + (16 * i));
          if(send_write >= 31)
            send_write = 0;
        }
      }
      while(Serial1.available())
      {
        Serial1.readBytes(&vide, 1);
      }
    } 
   debug_println(" end writing");
   //Flag_request = false;
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
  Serial1.begin(115200);
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


