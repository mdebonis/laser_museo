/*  versione 7 accende il laser e lo muove secondo il parametro inserito "acceso/spento"
 *  versione 6. impostazioni delle posizioni del servo tramite web server 
 *  la versione 5 memorizza i movimenti compiuti tramite interfaccia web nel file di testo inserendo anche il tempo e 
 * gli altri parametri: laser spento o acceso (se il laser è acceso il movimento sarà anche lento) e la coordinata (come angolo)
 * la versione 4 espone un server web mediante il quale è possibile far compiere i movimenti in
//manuale al servo, impostando da una pagina web gli angoli. Occorre collegarsi al wifi ESPMUSEUM con password museum57 e
//con il browser cercare la pagina http://192.168.4.1
//FUNZIONA. Il tutto parte premendo un pulsante, mentre quando finisce l'audio ripremendo il pulsante
//il sistema si resetta e poi per far ripartire ilciclo occorre di nuovo premere il pulsante.
//servoaudiotask2 legge in un file in sd la posizioni del servo mentre scorre l'audio
//il formato del file di testo sarà il seguente: tempo di ritardo prima di compiere il movimento, posizione x, y 
//in coordinate cartesiane, laser acceso o spento durante il percorso. p.es:
//03;120;050;1   aspetta 3 sec. poi si posiziona su x=120 e y=50 con laser acceso con campi fissi
//il programma legge una riga, effettua il movimento e poi legge la successiva
*/
#include <Servo.h>
#include "Arduino.h"
#include "Audio.h"
#include "SPI.h"
#include "SD.h"
#include "FS.h"
//per web server
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <WiFiAP.h>
#include <HTTPClient.h>

// Digital I/O used
#define SD_CS          5
#define SPI_MOSI      23
#define SPI_MISO      19
#define SPI_SCK       18
#define I2S_DOUT      25
#define I2S_BCLK      27
#define I2S_LRC       26

Audio audio;
static const int servoPin = 4;
static uint8_t buf[512];
static int puls = 0;   //pulsante
static int laser = 2;  //laser
Servo servo1;
bool attesaRestart = false; 
bool setServo = true;  //se non si è premuto il pulsante, mi permette di settare le posizioni del servo e memorizzare in file tramite web server
bool primoStart = true;
int len = 0;       //lunghezza del file servo
long l = 0;
//int angolo = 0; 
int angoloMem = 0;
int posInit = 90;   //posizione iniziale
String laserOn = "";  //se "1" vuol dire che il laser, durante il movimento rimane acceso e il posizionamento del laser avviene lentamente
char FORMservo[] = "<form method=\"POST\" action= \"/rispFORMservo\"><br><H2>MUSEUM</H2><br><br>angolo<input type=\"text\" name=\"ang\"><br>deltaT<input type=\"text\" name=\"delt\"><br>laser acceso<input type=\"radio\" name=\"las\" value=\"lasOn\"><br>acquisisci in file<input type=\"radio\" name=\"fil\" value=\"inFile\"><br><input type=\"submit\" value=\"CONTINUA\"></H2></form>";
char FORMciclo[200];
String formCiclo1 = "<form method=\"POST\" action= \"/rispFORMservo\"><br><H2>Valore angolo attuale: ";
String formCiclo2 = "<br><br>nuovo angolo<input type=\"text\" name=\"ang\"><br>deltaT<input type=\"text\" name=\"delt\"><br>laser acceso<input type=\"radio\" name=\"las\" value=\"lasOn\"><br>acquisisci in file<input type=\"radio\" name=\"fil\" value=\"inFile\"><br><input type=\"submit\" value=\"CONTINUA\"></H2></form>";
                                           
AsyncWebServer server(80);
const char *ssid = "esp32museum";
const char *password = "museum57";

void setup() {
    Serial.begin(115200);
    pinMode(puls, INPUT);
    pinMode(SD_CS, OUTPUT);  
    pinMode(laser, OUTPUT); 
    digitalWrite(SD_CS, HIGH);
    SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI);   
    SPI.setFrequency(1000000);
    SD.begin(SD_CS);
    servo1.attach(servoPin);
    audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
    audio.setVolume(12); // 0...21

    WiFi.softAP(ssid, password);
    IPAddress myIP = WiFi.softAPIP();
    Serial.print("AP IP address: ");
    Serial.println(myIP);
    delay(1000);
//    audio.connecttoSD("/320k_test.mp3");

 xTaskCreate(
                    voice,          /* Task function. */
                    "voice",        /* String with name of task. */
                    10000,            /* Stack size in bytes. */
                    NULL,             /* Parameter passed as input of the task */
                    1,                /* Priority of the task. */
                    NULL);            /* Task handle. */
 
  xTaskCreate(
                    servo,          /* Task function. */
                    "servo",        /* String with name of task. */
                    10000,            /* Stack size in bytes. */
                    NULL,             /* Parameter passed as input of the task */
                    1,                /* Priority of the task. */
                    NULL);            /* Task 0handle. */

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
  {
    request->send(200, "text/html", FORMservo);
  }); 
 //acquisisce l'angolo inviato dal form su browser    
  server.on("/rispFORMservo", HTTP_POST, [](AsyncWebServerRequest *request)
  { int angolo = -1; //se non inserisco il campo nel form il servo non si deve muovere
    Servo servo2;
    servo2.attach(servoPin);
    String angoloStr, deltaT, laserOn = "0";
    int paramsNr = request->params();
    Serial.println(paramsNr);
    for(int i=0;i<paramsNr;i++)
    { 
        AsyncWebParameter* p = request->getParam(i);
//acquisisce dal form l'angolo
        if (p->name()=="ang")    
           {angolo = p->value().toInt();
           angoloStr = p->value();
           Serial.print("angolo; ");  Serial.println(angolo);  
           }  
        if (p->name()=="delt")  
           {deltaT = p->value();}  //deltaT è un stringa dovendolo inserire nel file
        if (p->name()=="las")  
           {if (p->value()=="lasOn") laserOn = "1"; }
        if (p->name()=="fil")  
           {if (p->value()=="inFile")
              { Serial.println(angoloStr); Serial.println(laserOn);
                String s = ""; Serial.println(s);
                s += deltaT;  Serial.println(s);
                s += ";";      Serial.println(s);
                s += angoloStr;  Serial.println(s);
                s += ";";        Serial.println(s);
                s += laserOn;    Serial.println(s);        
               char message[15];                                              Serial.println("x");
               int lung = s.length();                                       
               for (int i=0; i< lung;i++) message[i] = s[i];                 
               message[lung] = 0xD;                                          
               message[lung+1] = 0xA;                                         
               message[lung+2] = '\0';                                       
               appendFile(SD, "/servo.txt", message);
           }
        }
    }
    String s1 = formCiclo1 + angoloStr + formCiclo2;
    for (int i =0; i< s1.length(); i++)
         FORMciclo[i] = s1[i];
    request->send(200, "text/html", FORMciclo); 
    digitalWrite(laser, LOW);
    servo2.write(angolo);
    delay(500);
    digitalWrite(laser, HIGH);

  }); 
            
   void disableCore0WDT();
   void disableCore1WDT();
   void disableLoopWDT();
  server.begin();
  Serial.println("Server started");
//posiziona il servo al centro
  servo1.write(posInit);  
}
// 
void loop() {
//vTaskDelay(10); 
}

void voice( void * parameter )
{
 //attesaRestart si setta dopo che l'audio è stato riprodotto completamente
    for(;;){
     if(attesaRestart){ while(true)
            { delay(1000);  //altrimenti va in WD e riparte
             if (digitalRead(puls) == LOW) ESP.restart();
            }
           }
    if (primoStart && digitalRead(puls) == LOW) 
       {audio.connecttoSD("/320k_test.mp3");    Serial.println("pulsante premuto");
        primoStart = false;
       }
    else if (primoStart) delay(500);     
    audio.loop();
    l++; // Serial.println(l);
    if (l > 26000) 
       {audio.stopSong(); 
       l = 0;
       attesaRestart = true;
       digitalWrite(laser, LOW);
       }
    }
}
 
void servo( void * parameter)
{
  for (;;)
  { 
     if(setServo && digitalRead(puls) ==LOW)
     {digitalWrite(laser, LOW);
      servo1.write(posInit);
      setServo = false;  Serial.println("pulsante premuto2");
      int i = 0;
      String riga;
      readFile(SD, "/servo.txt");
      int posMem = posInit;
     while (i < len)
      { riga = ""; 
       while (buf[i] != 0xD)
        {  riga += (char)buf[i];
           i++;
        }   
       Serial.println(riga); 
       int tempo = riga.substring(0,2).toInt(); // tempo prima di posizionarsi
       int pos = riga.substring(3,6).toInt(); // posizione in gradi
       char moving = riga[7];   //acceso = '1' e movimento lento, spento  = '0' e movimento veloce
       Serial.println(tempo);
       Serial.println(pos);
       Serial.println(moving);
       delay(tempo*1000);       //in ms
       if (moving == '0')
         {   digitalWrite(laser, LOW);
             servo1.write(pos);
             delay(600);  //tempo di posizionamento
             digitalWrite(laser, HIGH);
         }
       if (moving == '1')
         {   digitalWrite(laser, HIGH);
             for (int j = posMem; j < pos; j=j+2)
             {servo1.write(j);
             delay(100);  //tempo di posizionamento
             }
         }       
       i += 2;  //come fine riga c'è 0xD 0xA
      }
     }
   delay(500);
//   Serial.println("ok");
  }
}

void wifi( void * parameter )
{
  for(;;){
      
    }
}

void readFile(fs::FS &fs, const char * path){
    Serial.printf("Reading file: %s\n", path);

    File file = fs.open(path);
    if(!file){
        Serial.println("Failed to open file for reading");
        return;
    }
      len = file.size();               //   Serial.println(len);   
      file.read(buf,len);
    file.close();
}
void appendFile(fs::FS &fs, const char * path, const char * message){
    Serial.printf("Appending to file: %s\r\n", path);

    File file = fs.open(path, FILE_APPEND);
    if(!file){
        Serial.println("- failed to open file for appending");
        return;
    }
    if(file.print(message)){
        Serial.println("- message appended");
    } else {
        Serial.println("- append failed");
    }
}
//void setServoByWebServer(int ang)
//{ if (ang != angoloMem)
//     { Serial.print("start servo  "); Serial.println(ang); 
//       servo1.write(angolo);
//       delay(100);  //tempo di posizionamento
//       angoloMem = ang;
//     }
// return; 
//}
