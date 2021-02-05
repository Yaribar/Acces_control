#include <Arduino.h>
#include <WiFi.h> //This library allows ESP32 to connect to the Internet
#include <PubSubClient.h> //This library provides a client for doing simple publish/subscribe messaging with a server that supports MQTT.

#define RXD2 16 //Hardware Serial
#define TXD2 17 //Hardware Serial
#define LED_BUILTIN 2

TaskHandle_t Task1;//The implementation of other task that apper to happen at the same time is done with RTOS

const String serial_number = "123456789";

//const char* ssid = "Telcel-HUAWEI-B311-520-BFA6"; //WiFi Network
//const char* password = "95HNEAD2ERD"; //WiFi's password

const char* ssid = ""; //WiFi Network
const char* password = ""; //WiFi's password

//To avoid the DHCP assign us an IP or the router dont have DHCP we can select a fix ip
//IPAddress local_IP(192,168,0,184);
//IPAddress gateway(192,168,0,184);
//IPAddress subnet(255,255,255,0);
//IPAddress primaryDNS(8,8,8,8);
//IPAddress secondaryDNS(8,8,4,4);

//*************************
//** MQTT CONFIGURATION ***
//*************************

const char *mqtt_server = "c-iot.ml"; //Domain name or IP
const uint16_t mqtt_port = 1883; //MQTT port TCP
const char *mqtt_user = "web_client"; //MQTT user
const char *mqtt_pass = "121212"; //MQTT password

WiFiClient espClient; //Create an object WiFi to connect to internet
PubSubClient client(espClient); //The MQTT protocol will work with the connection done through WiFi

long lastMsg = 0; //Variable to save last message
char msg[25]; //Character array to receive message
bool send_access_query = false;

String rfid = ""; //Save rfid
String user_name = ""; //save user name

//*************************
//**FUNCTION DECLARATION***
//*************************

void acces(bool access);
void callback(char* topic, byte* payload, uint length);
void closing();
void iddle();
void opening();
void reconnect();
void sending();
void setup_wifi();

//************************
//**INTERNAL TEMP SENSOR**
//************************

#ifdef __cplusplus
extern "C" {
	#endif

	uint8_t temprature_sens_read();

	#ifdef __cplusplus
}
#endif

uint8_t temprature_sens_read();

//**********************
//**TASK IN OTHER CORE**
//**********************

void codeForTask1(void *parameter){
    for(;;){
        while(Serial2.available()){
            rfid += char(Serial2.read());
        }
        if(rfid!=""){
            Serial.println("Se solicita acceso para tarjeta ->"+ rfid);
            send_access_query = true;
            delay(1000);
        }

        vTaskDelay(10);

    }
}

//*************************
//****     SETUP     ******
//*************************

void setup() {
    pinMode(LED_BUILTIN,OUTPUT); //LED_BUILTIN AS OUTPUN
    Serial.begin(115200); //initialize serial port
    Serial2.begin(9600,SERIAL_8N1,RXD2,TXD2);//initializa hardware serial 2
    randomSeed(micros()); // number to initialize the pseudo-random sequence

    xTaskCreatePinnedToCore(
        codeForTask1, /*Task function*/
        "Task_1",   /*Name of task*/
        1000,   /*Stack size of task*/
        NULL,   /*Parameter of the task*/
        1,  /*priority of the task*/
        &Task1, /*Track handle to keep track of created task*/
        0); /*Core*/
    
    iddle();
    
    setup_wifi(); //Connects to wifi
    client.setServer(mqtt_server, mqtt_port); //Identifies the server and port to be connected
    client.setCallback(callback);//Whenever a message arrives we call this fucntion
}

//*************************
//****      LOOP     ******
//*************************

void loop() {
    if(!client.connected()){//Check if the board is connected to the server
        reconnect();
    }

    client.loop(); //This method checks the messages, connection, etc

    long now = millis(); //this variable is useful to set a sample time
    if(now - lastMsg > 2000){
        lastMsg = now;
        String msg_send = String((temprature_sens_read() - 32) / 1.8);
        msg_send.toCharArray(msg, 25);//As the library requires a char array it's neccesary to trasform the string
        
        char topic[25];
        String topic_aux = serial_number + "/temp";
        topic_aux.toCharArray(topic,25);

        client.publish(topic,msg);//publish the message with the topic values and the message
    }

    if(send_access_query){
        
        String msg_send = rfid;
        rfid = "";

        sending();
        msg_send.toCharArray(msg, 25);//As the library requires a char array it's neccesary to trasform the string
        
        char topic[25];
        String topic_aux = serial_number + "/access_query";
        topic_aux.toCharArray(topic,25);

        client.publish(topic,msg);//publish the message with the topic values and the message
        
        send_access_query = false;
    }

}

//*************************
//****ACCESS MESSAGE*******
//*************************
void access(bool access){
    if(access){
        Serial.println("Hola" + user_name);
        Serial.println("");
        Serial.println("ACCESS GRANTED");
        Serial.println("");
        delay(2000);
        opening();
    }else{
        Serial.println("ACCESS DENIED");
        Serial.println("");
        delay(2000);
        iddle();
    }
}

//*************************
//*******  OPEN  **********
//*************************

void opening(){
    Serial.println("OPENING...");
    Serial.println("");
    delay(2000);
    iddle();
}

//*************************
//*******  CLOSING  *******
//*************************

void closing(){
    Serial.println("CLOSING...");
    Serial.println("");
    delay(2000);
    iddle();
}

//*************************
//******  SENDING   *******
//*************************

void sending(){
    Serial.print("HOLD ON");
    delay(300);
    Serial.print(".");
    delay(300);
    Serial.print(".");
    delay(300);
    Serial.println(".");
    delay(300);
}

//*************************
//*******  IDDLE   ********
//*************************

void iddle(){
    Serial.println("ENTER CARD");
    Serial.println("");
}

//*************************
//*****WIFI CONNECTION*****
//*************************
void setup_wifi(){
    delay(10);

    //if(!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS)){
    //    Serial.println("STA Failed to configure");
    //}

    Serial.println();
    Serial.printf("Connecing to %s",ssid);

    WiFi.begin(ssid,password);

    while (WiFi.status() != WL_CONNECTED){ //If there is no connection to the network the procces won't continue
        delay(500);
        Serial.print(".");
    }
    Serial.printf("\nConnected to WiFi network!\n");
    Serial.println("IP: ");
    Serial.println(WiFi.localIP());
}

//*************************
//******  LISTENER  *******
//*************************

void callback(char* topic, byte* payload, uint length){
    String incoming = "";
    Serial.print("Message received from -> ");
    Serial.print(topic);
	Serial.println("");

    //As date is send through char array this for concatenate the information in the String "incoming"
    for (int i = 0; i < length; i++)
    {
        incoming += (char)payload[i];
    }
    incoming.trim(); //Get a version of the String with any leading and trailing whitespace removed
    Serial.println("Mensaje ->" + incoming);

    String str_topic(topic);
    if(str_topic == serial_number + "/command"){
        if (incoming=="open"){
            digitalWrite(LED_BUILTIN, HIGH);
            opening();
        }
        if (incoming=="close"){
            digitalWrite(LED_BUILTIN, LOW);
            closing();
        }
        if (incoming=="granted"){
            digitalWrite(LED_BUILTIN, HIGH);
            access(true);
        }
        if (incoming=="refused"){
            digitalWrite(LED_BUILTIN, LOW);
            access(false);
        }
            
    }
    if(str_topic == serial_number + "/user_name"){
        user_name = incoming;
    }
}

//*************************
//****CONNECTION MQTT******
//*************************

void reconnect(){
    while(!client.connected()){
        Serial.print("Trying connection MQTT...");
        //Create a client ID
        String clientId = "esp32_";
        clientId += String(random(0xffff),HEX);//If there are frequent disconnections this allow the board to connect with a different ID
                                
        if(client.connect(clientId.c_str(),mqtt_user,mqtt_pass)){//connects to MQTT
            Serial.println("Connected!");

            char topic[25];
            String topic_aux = serial_number + "/command";
            topic_aux.toCharArray(topic,25);
            client.subscribe(topic);//subscribe to topic command

            char topic2[25];
            String topic_aux2 = serial_number + "/user_name";
            topic_aux2.toCharArray(topic2,25);
            client.subscribe(topic2);//subscribe to topic  user_name
            
        }
        else{ //If the conection fails try again
            Serial.print("fail :( with error -> ");
            Serial.print(client.state());
            Serial.println(" Try again in 5 seconds");

            delay(5000);
        }
    }
}
