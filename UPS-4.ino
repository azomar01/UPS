#include <ESP8266WiFi.h> 
#include <ESP8266WebServer.h> 
#include <FS.h> 
#include <DNSServer.h>
#include <ESP8266mDNS.h>
#include <PubSubClient.h>
#include <OneWire.h> 
#include <ArduinoOTA.h> 
#include <DallasTemperature.h> 
#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>
#include <WiFiUdp.h> 
// ================= WIFI ================= 

 String wifiMode = "0",msg,s; 
 String ssid = "NUMOTRONIC";
 String password = "12345678"; 
 ESP8266WebServer server(80); 
 DNSServer dnsServer;
  #define DNS_PORT 53 

// ================= MQTT ================= 

 WiFiClient espClient;
 PubSubClient mqttClient(espClient);
 String chipID;
 String mqttServer = "test.mosquitto.org";
 String mqttPort = "1883"; String mqttUser = "";
 String mqttPass = ""; 
 String mqttSubTopic = "6046fe26c9f043a18f09a3fe337ebde5",SUBTOPIC;
 String mqttPubTopic = "6046fe26c9f043a18f09a3fe337ebde5",PUBTOPIC;
 bool mqttEnable = 1 ; bool mqttConnected = false ; 

// ================= UPS ================= 

     #define PIN_MAINS    14
     #define PIN_BYPASS   12
     #define PIN_BATT_LOW 13
     #define PIN_VAR      16
     #define PIN_FAULT     5
     #define PIN_K1        0 
     #define PIN_LED       2 

// ================= TEMP =================
 #define ONE_WIRE_BUS 4

 OneWire oneWire(ONE_WIRE_BUS); 
 DallasTemperature sensors(&oneWire); 
 float tempUPS = 0; float tempAMB = 0; 

// ================= VAR ================= 
unsigned long currentMillis = millis();
 int interval=60,DELAY; 
 unsigned long previousMillis = 0;
  bool ledState = false; 
  bool apActive = false; 
  unsigned long apLastClientTime = 0; 
  unsigned long k1PressTime = 0;
  bool k1Handled = false;
  unsigned long lastSend = 0;

 // ================= HTTPUPDATE ================= 
  void UPDATE(){
     unsigned long tim; String VER ; VER=server.arg("data"); Serial.println (VER);
     
      WiFi.disconnect(); WiFi.mode(WIFI_STA); WiFi.begin("NUMOTRONIC","12345678"); tim=millis();
      
      while ((WiFi.status() != WL_CONNECTED)&&(millis()<(tim+15000))) {   delay(100);}
         
      if (WiFi.status() == WL_CONNECTED) {delay(500); tim=millis();
      while (millis()<=tim+10000){ 
      WiFiClientSecure client; client.setInsecure(); ESPhttpUpdate.setLedPin(PIN_LED, LOW);
       t_httpUpdate_return ret = ESPhttpUpdate.update(client, "https://raw.githubusercontent.com/azomar01/UPS/master/"+String(VER)+".bin"); delay(100); } } } 

 
 
 //================= SPIFFS =================
  void saveConfig(){
     File f=SPIFFS.open("/config.txt","w");
      f.println(wifiMode); 
      f.println(ssid);
      f.println(interval);      
      f.println(password);
      f.println(mqttServer); 
      f.println(mqttPort); 
      f.println(mqttUser); 
      f.println(mqttPass); 
      f.println(mqttSubTopic); 
      f.println(mqttPubTopic); 
      f.println(mqttEnable); 
      f.close(); } 
      
  
 //================= SPIFFS =================

      void loadConfig(){
      if (!SPIFFS.exists("/config.txt")) return; 
      File f= SPIFFS.open("/config.txt","r") ; 
      wifiMode=f.readStringUntil('\n');      wifiMode.trim(); 
      ssid= f.readStringUntil('\n');         ssid.trim();
      interval= f.readStringUntil('\n').toInt();  if(interval<10 || interval>1000)interval=60;
      password=f.readStringUntil('\n');      password.trim();
      mqttServer=f.readStringUntil('\n');    mqttServer.trim();
      mqttPort=f.readStringUntil ('\n');     mqttPort.trim();
      mqttUser=f.readStringUntil ('\n');     mqttUser.trim(); 
      mqttPass=f.readStringUntil ('\n');     mqttPass.trim(); 
      mqttSubTopic=f.readStringUntil('\n');  mqttSubTopic.trim(); 
      mqttPubTopic=f.readStringUntil('\n');  mqttPubTopic.trim(); 
      mqttEnable= f.readStringUntil('\n').toInt(); 
      if(mqttEnable!=0 && mqttEnable!=1) mqttEnable=1;f.close(); } 
 
 // ================= UPS ================= 

 bool readPin(int pin){ return digitalRead(pin) == LOW; }

  // ================= TEMP ================= 
  void readTemps(){ sensors.requestTemperatures(); tempUPS = sensors.getTempCByIndex(0); tempAMB = sensors.getTempCByIndex(1); } 
 
 // ================= MQTT CALLBACK =================
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  Serial.println(topic);
  String msg;
  for (unsigned int i = 0; i < length; i++) {
    msg += (char)payload[i];
  }

  Serial.print("MQTT CMD: ");
  Serial.println(msg);

  // ===== RESET =====   "reset"
  if (msg.indexOf("\"reset\"") != -1) {
    
     reconnectMQTT(); mqttClient.loop();
       s="RESET...";
       mqttClient.publish(PUBTOPIC.c_str(), s.c_str());
       Serial.println("RESET...");
    delay(5000);ESP.restart();
  }

  // ===== UPDATE OTA =====   "update","ver":"UPS_3"
  else if (msg.indexOf("\"update\"") != -1) {

    int p = msg.indexOf("\"ver\":\"");
    if (p != -1) {
      String ver = msg.substring(p + 7);
      ver = ver.substring(0, ver.indexOf("\""));
      
      reconnectMQTT(); mqttClient.loop();
       s="UPDATE...VER:";s+=ver;
       mqttClient.publish(PUBTOPIC.c_str(), s.c_str());delay(5000);
       
      Serial.println("UPDATE: " + ver);

      WiFiClientSecure client;
      client.setInsecure();ESPhttpUpdate.setLedPin(PIN_LED, LOW);

      String url = "https://raw.githubusercontent.com/azomar01/UPS/master/" + ver + ".bin";
      ESPhttpUpdate.update(client, url);
    }
  }

  // ===== CHANGE WIFI =====  "set_wifi""ssid":"xxXXXXxx","pass":"yyYYYYyy"
  else if (msg.indexOf("\"set_wifi\"") != -1) {   
      
       reconnectMQTT(); mqttClient.loop();
       s="LAST WIFI: " + ssid +","+ password+"  \"CONNECTED !!\"";
       mqttClient.publish(PUBTOPIC.c_str(), s.c_str());delay(5000);

    int si = msg.indexOf("\"ssid\":\"");
    int p = msg.indexOf("\"pass\":\"");

    if (si != -1 && p != -1) {

      ssid = msg.substring(si + 8);
      ssid = ssid.substring(0, ssid.indexOf("\""));

      password = msg.substring(p + 8);
      password = password.substring(0, password.indexOf("\""));

      Serial.println("NEW WIFI: " + ssid +","+  password);  
      startWiFi();       
      
      if(WiFi.status() == WL_CONNECTED){saveConfig();
       reconnectMQTT(); mqttClient.loop();
       s="NEW WIFI: " + ssid +","+ password+"CONNECTED !!";
       mqttClient.publish(PUBTOPIC.c_str(), s.c_str());delay(5000); MDNS.begin("upstracker"); }
                   
                   
                    else{    loadConfig(); 
                             startWiFi(); mqttClient.setServer(mqttServer.c_str(), mqttPort.toInt()); 
                             mqttClient.setCallback(mqttCallback);reconnectMQTT(); mqttClient.loop();
                             s="FAIL TO CONNECT NEW WIFI: ";
                             mqttClient.publish(PUBTOPIC.c_str(), s.c_str());delay(5000);}
    } 
  }

  // ===== CHANGE MQTT =====     "set_mqtt""server":"XXXXxxxx","port":"yyYYYYyy"
  else if (msg.indexOf("\"set_mqtt\"") != -1) {

    int si = msg.indexOf("\"server\":\"");
    int po = msg.indexOf("\"port\":\"");

    if ((si != -1)&&(po != -1)) {
      mqttServer = msg.substring(si + 10);
      mqttServer = mqttServer.substring(0, mqttServer.indexOf("\""));
    
      mqttPort = msg.substring(po + 8);
      mqttPort = mqttPort.substring(0, mqttPort.indexOf("\""));
    
    Serial.println("MQTT UPDATED  :");   
      s= "mqttServer:  "+mqttServer ;
     Serial.println(s); s="mqttPort:  "+mqttPort;Serial.println(s);  

       reconnectMQTT(); mqttClient.loop();
       s="NEW SERVER: " + mqttServer +","+ mqttPort;
       mqttClient.publish(PUBTOPIC.c_str(), s.c_str()); delay(5000);

    mqttClient.setServer(mqttServer.c_str(), mqttPort.toInt());
    mqttClient.disconnect();
    
    }
   // saveConfig();    

  }

  // ===== INTERVAL =====     "interval","value":10
  else if (msg.indexOf("\"interval\"") != -1) {

    int v = msg.indexOf("\"value\":");
    if (v != -1) {

      String val = msg.substring(v + 8);
       if(val.toInt()>=10 && val.toInt()<=1000) { interval = val.toInt();saveConfig();s="NEW INTERVAL: " +String(interval);}
                               else {s="FAIL INTERVAL DATA ";}
      Serial.println("NEW INTERVAL: " + String(interval));
       reconnectMQTT(); mqttClient.loop();
       
       mqttClient.publish(PUBTOPIC.c_str(), s.c_str());delay(5000);
    }
  }
  // ===== START AP =====     "start-ap"
  else if (msg.indexOf("\"start-ap\"") != -1) {
       reconnectMQTT(); mqttClient.loop();
       s="START AP !! " ;
       mqttClient.publish(PUBTOPIC.c_str(), s.c_str());delay(5000);
     startAP();
   }

}// ================= MQTT ================= 
 
 void reconnectMQTT(){ 

  if(!(mqttEnable && wifiMode=="1" && WiFi.status()==WL_CONNECTED)) return;

   if(mqttClient.connected()) return;

   if(mqttClient.connect("UPSTRACKER", mqttUser.c_str(), mqttPass.c_str())){

     mqttConnected = true;//Serial.println("mqtt-Connected"); 
     mqttClient.subscribe(SUBTOPIC.c_str()); } 

     else
      { mqttConnected = false;//Serial.println("mqtt-Disconnected");
       } } 
     
     
     void sendMQTT(){ if(!mqttEnable) return;
      readTemps(); 

             msg="\"device_id\": \""+chipID+"\", \"params\": {"; 
             msg+="\"bypass\":"+ String(readPin(PIN_BYPASS))+",";
             msg+="\"secteur\":"+ String(!readPin(PIN_MAINS))+",";
             msg+="\"variateur\":"+ String(!readPin(PIN_VAR))+","; 
             msg+="\"low-battery\":"+  String(readPin(PIN_BATT_LOW))+",";
             msg+="\"dead-battery\":"+ String(readPin(PIN_FAULT))+",";
             msg+="\"generique\":\"01\","; 
             msg+="\"TMPups\":"; 
             msg+="36,";                   //String(tempUPS,1)+",";
             msg+="\"TMPam\":"; msg+="24"; //String(tempAMB,1); 
             
             msg+="}"; 
       
       
       mqttClient.publish(PUBTOPIC.c_str(), msg.c_str());
       
        Serial.println("envoie du data"); } 
 
 // ================= WIFI START (STA ONLY) ================= 
 void startWiFi(){ 
  WiFi.softAPdisconnect(true); 

  WiFi.disconnect(true); delay(500);wifiMode="1";

  WiFi.mode(WIFI_STA); 
  WiFi.begin(ssid.c_str(), password.c_str()); 
  unsigned long t = millis(); 
  while(WiFi.status() != WL_CONNECTED && millis() - t < 15000){ 
    delay(500); } if(WiFi.status() == WL_CONNECTED){ MDNS.begin("upstracker"); }
     } 
 
 // ================= AP TEMPORARY ================= 
void startAP(){
  if(apActive) return;
  
  WiFi.softAPdisconnect(true);
  WiFi.disconnect(true);

  WiFi.mode(WIFI_AP_STA);
  msg="UPS-controle--"+chipID;
  WiFi.softAP(msg.c_str(), "12345678");wifiMode="0";
  Serial.println("WIF-AP-START");

  dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());

  apActive = true;
  apLastClientTime = millis();
}
 
  // ================= STOP AP ================= 
  void stopAP(){ 
    if(!apActive) return; 
    dnsServer.stop();Serial.println("WIF-AP-STOP"); WiFi.softAPdisconnect(true); 
    startWiFi(); apActive = false; } 
  
  // ================= CHECK CLIENTS =================
   void checkAPClients(){
     if(!apActive) return; if(WiFi.softAPgetStationNum() > 0)
     { apLastClientTime = millis(); }
      if(millis() - apLastClientTime > 120000){ stopAP(); } } 
  
  // ================= K1 BUTTON ================= 
  void checkK1(){ 
    if(digitalRead(PIN_K1) == LOW){ 
      if(k1PressTime == 0) k1PressTime = millis();
       if(!k1Handled && millis() - k1PressTime > 5000){ 
        startAP(); k1Handled = true; } } 
        else { k1PressTime = 0; k1Handled = false; } } 
  
// ================= STATUS API =================
   void handleStatus(){
     readTemps(); String json="{"; 
     json+="\"mains\":"+ String(!readPin(PIN_MAINS))+",";
     json+="\"bypass\":"+ String(readPin(PIN_BYPASS))+","; 
     json+="\"var\":"+ String(readPin(PIN_VAR))+",";
     json+="\"low\":"+ String(readPin(PIN_BATT_LOW))+",";
     json+="\"fault\":"+ String(readPin(PIN_FAULT))+",";
     json+="\"t1\":"+ String(tempUPS,1)+","; 
     json+="\"t2\":"+ String(tempAMB,1)+","; 
     json+="\"mqtt\":"+ String(mqttConnected)+","; 
     json += "\"chipID\":\"" + chipID + "\","; 
     json+="\"enabled\":"+ String(mqttEnable); 
     json+="}"; server.send(200,"application/json",json); } 
  
  // ================= handleRoot ================= 
  void handleRoot(){ String page; 
  page += "<!DOCTYPE html><html><head>"; page += "<meta name='viewport' content='width=device-width, initial-scale=1'>"; 
  page += "<style>"; page += "body{background:#0f172a;color:white;font-family:Arial;text-align:center}"; 
  page += ".card{background:#1e293b;margin:10px;padding:15px;border-radius:12px}"; 
  page += ".ok{color:#22c55e;font-weight:bold}"; 
  page += ".fail{color:#ef4444;font-weight:bold}";
  page += ".off{color:gray;font-weight:bold}"; 
  page += "button{padding:10px;border:none;border-radius:8px;background:#38bdf8;color:white}";
  page += ".id{font-size:20px;font-weight:bold;color:#38bdf8;margin-top:8px}";
  page += "</style></head><body>"; page += "<h2>UPS_3 MONITOR </h2>"; // DEVICE ID
  page += "<div class='card'>DEVICE-ID"; 
  page += "<div id='I' class='id'></div>"; 
  page += "</div>"; page += "<div class='card'>SECTEUR <div id='m'></div></div>"; 
  page += "<div class='card'>BYPASS <div id='b'></div></div>";
  page += "<div class='card'>ON BATTRY <div id='v'></div></div>"; 
  page += "<div class='card'>BAT LOW <div id='l'></div></div>"; 
  page += "<div class='card'>BAT FAULT <div id='f'></div></div>";
  page += "<div class='card'>UPS TEMP <div id='t1'></div></div>"; 
  page += "<div class='card'>AMBIENT TMP <div id='t2'></div></div>";
  page += "<div class='card'>SERVER <div id='mq'></div></div>"; 
  page += "<a href='/settings'><button>CONFIG</button></a>"; 
  page += "<script>";
  page += "setInterval(()=>{fetch('/status').then(r=>r.json()).then(d=>{"; // STATUS 
  page += "m.innerHTML=d.mains?'<span class=ok>OK</span>':'<span class=fail>FAIL</span>';"; 
  page += "b.innerHTML=d.bypass?'<span class=ok>OK</span>':'<span class=fail>FAIL</span>';";
  page += "v.innerHTML=d.var?'<span class=ok>OK</span>':'<span class=fail>FAIL</span>';"; 
  page += "l.innerHTML=d.low?'<span class=ok>OK</span>':'<span class=fail>FAIL</span>';"; 
  page += "f.innerHTML=d.fault?'<span class=ok>OK</span>':'<span class=fail>FAIL</span>';";
  page += "t1.innerHTML=d.t1+' C';"; page += "t2.innerHTML=d.t2+' C';"; // MQTT 
  page += "mq.innerHTML=d.enabled?(d.mqtt?'<span class=ok>CONNECTED</span>':'<span class=fail>OFF</span>'):'<span class=off>DISABLED</span>';"; // CHIP ID 
  page += "I.innerHTML=d.chipID;"; page += "})},1000);"; 
  page += "</script>"; 
  page += "</body></html>"; 
  server.send(200,"text/html",page); } 
  
  // ================= CONFIG =================
   void handleSettings(){ 
    String page="<!DOCTYPE html><html><head>"; 
    page+="<meta name='viewport' content='width=device-width,initial-scale=1'>"; 
    page+="<style>"; page+="body{font-family:Arial;background:#eef;text-align:center}"; 
    page+=".c{background:#fff;width:320px;margin:20px auto;padding:20px;border-radius:10px;box-shadow:0 3px 10px #aaa}"; 
    page+="input,select{width:100%;padding:8px;margin:5px 0}"; page+="button{width:100%;padding:10px;margin-top:10px;border:0;color:#fff}"; 
    page+=".s{background:#3498db}.r{background:#555}.b{background:#888}"; page+="</style></head><body>";
    page+="<div class='c'><h3>CONFIG UPS</h3><form action='/save'>"; 
    //page+="Mode<select name='mode'>";//<option value='0'"+String(wifiMode=="0"?" selected":"")+">AP</option>";
    //page+="<option value='1'"+String(wifiMode=="1"?" selected":"")+">STA</option></select>"; 
    page+="SSID<input name='ssid' value='"+ssid+"'>"; 
    page+="PASS<input name='pass' value='"+password+"'>";
    page+="Etat Serveur: "; 
         if(!mqttEnable)  page+="<b style='color:gray'>DESACTIVE</b>"; 
    else if(mqttConnected)  page+="<b style='color:green'>CONNECTE</b>";
     else page+="<b style='color:red'>DISCONNECTE</b>"; 
     page+="<h4>Serveur </h4>"; 
     page+="ON/OFF<select name='mqtten'>"; 
     page+="<option value='1'"+String(mqttEnable==1?" selected":"")+">ON</option>"; 
     page+="<option value='0'"+String(mqttEnable==0?" selected":"")+">OFF</option>"; 
     page+="</select>"; page+="Server<input name='mserver' value='"+mqttServer+"'>"; 
     page+="Port<input name='mport' value='"+mqttPort+"'>"; page+="User<input name='muser' value='"+mqttUser+"'>"; 
     page+="Pass<input name='mpass' value='"+mqttPass+"'>"; page+="Topic SUB<input name='msub' value='"+mqttSubTopic+"'>"; 
     page+="Topic PUB<input name='mpub' value='"+mqttPubTopic+"'>"; page+="<button class='s'>SAVE</button></form>"; 
     page+="<button class='r' onclick='location.reload()'>Actualiser</button>"; 
     page+="<a href='/'><button class='b'>Retour</button></a>"; page+="</div></body></html>"; 
     server.send(200,"text/html",page); } 
  
  // ================= SAVE ================= 
  void handleSave(){ 
    wifiMode= server.arg("mode"); 
    ssid= server.arg("ssid"); 
    password= server.arg("pass"); 
    mqttEnable= server.arg("mqtten").toInt(); 
    mqttServer= server.arg("mserver"); 
    mqttPort= server.arg("mport"); 
    mqttUser= server.arg("muser"); 
    mqttPass= server.arg("mpass"); 
    mqttSubTopic= server.arg("msub"); 
    mqttPubTopic= server.arg("mpub"); 
    saveConfig(); 
    loadConfig(); 
    startWiFi(); 
    mqttClient.setServer(mqttServer.c_str(), mqttPort.toInt()); }
  
  
   // ================= SETUP ================= 
   void setup(){ 
   
    Serial.begin(115200); SPIFFS.begin(); 
    Serial.println ("");
   
    pinMode(PIN_MAINS, INPUT_PULLUP); 
    pinMode(PIN_BYPASS, INPUT_PULLUP);
    pinMode(PIN_BATT_LOW, INPUT_PULLUP);
    pinMode(PIN_FAULT, INPUT_PULLUP);
    pinMode(PIN_K1, INPUT_PULLUP);
    pinMode(PIN_LED, OUTPUT); 
    chipID=String(ESP.getChipId()); 
        
     sensors.begin();
     loadConfig(); 
     startWiFi();

      server.on("/", handleRoot); 
      server.on("/status", handleStatus); 
      server.on("/settings", handleSettings); 
      server.on("/save", handleSave); 
      server.on("/update",UPDATE);
      server.begin();
      ArduinoOTA.begin();

      PUBTOPIC=mqttPubTopic+"/devices/"+chipID+"/status"; 
      SUBTOPIC=mqttSubTopic+"/devices/"+chipID+"/cmd";
      Serial.print("IDENTIFIANT UPS :");  Serial.println (chipID);
      Serial.print("mqtt-Sub :");  Serial.println (mqttSubTopic);
      Serial.print("mqtt-Pub :");  Serial.println (mqttPubTopic);
     
     mqttClient.setServer(mqttServer.c_str(), mqttPort.toInt()); 
     mqttClient.setCallback(mqttCallback);
     } 
   // ================= LOOP =================
    void loop(){ 
    server.handleClient(); 
    checkK1(); 
    checkAPClients(); 
    dnsServer.processNextRequest(); 

    if(wifiMode=="0"){    ArduinoOTA.handle();digitalWrite(PIN_LED, LOW);  dnsServer.processNextRequest();} 
     
      if(mqttEnable && wifiMode=="1" && WiFi.status()==WL_CONNECTED){

      if (!mqttClient.connected()) reconnectMQTT(); mqttClient.loop();
      
      if(millis()-lastSend>(interval*1000)){
            // if (!mqttClient.connected()) Serial.println("mqtt-disconnected"); 

             Serial.println(interval); 
             lastSend=millis(); sendMQTT(); } }

              else { mqttClient.disconnect(); mqttConnected=false; } 

              if (mqttConnected==true) { 
                       DELAY = 3000;// lent 
              } else { DELAY = 500;  // rapide 
              }
   if ((millis() - previousMillis >= DELAY)&& (wifiMode=="1")) {
                   previousMillis = millis(); digitalWrite(PIN_LED, LOW);
                   delay(200);digitalWrite(PIN_LED, HIGH); }
                   
                    }

