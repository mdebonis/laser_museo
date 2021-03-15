/*  versione 8: correzione bug, interruzione audio automatica, opzione eliminazione file movimenti da web
 *  versione 7: accende il laser e lo muove secondo il parametro inserito "acceso/spento"
 *  versione 6: impostazioni delle posizioni del servo tramite web server 
 *  versione 5: memorizza i movimenti compiuti tramite interfaccia web nel file di testo inserendo anche il tempo e 
 * gli altri parametri: laser spento o acceso (se il laser è acceso il movimento sarà anche lento) e la coordinata (come angolo)
 *  versione 4: espone un server web mediante il quale è possibile far compiere i movimenti in
 * manuale al servo, impostando da una pagina web gli angoli. Occorre collegarsi al wifi ESPMUSEUM con password museum57 e
 * con il browser cercare la pagina http://192.168.4.1
 *  FUNZIONA. Il tutto parte premendo un pulsante, mentre quando finisce l'audio ripremendo il pulsante
 *  il sistema si resetta e poi per far ripartire ilciclo occorre di nuovo premere il pulsante.
 *  Il programma legge in un file in sd la posizioni del servo mentre scorre l'audio.
 *  Il formato del file di testo sarà il seguente: tempo di ritardo prima di compiere il movimento, posizione x
 *  in coordinate cartesiane, laser acceso o spento durante il percorso. p.es:
 *  3;120;1   aspetta 3 sec. poi si posiziona su x=120 con laser acceso con campi a lunghezza variabile.
 *  Il programma legge una riga, effettua il movimento e poi legge la successiva
 */


//POTREBBE ESSERE NECESSARIO RIMUOVERE ALCUNE LIBRERIE STANDARD DI ARDUINO CHE HANNO LO STESSO NOME DELLE NUOVE LIBRERIE DA IMPORTARE
//LA PRESENZA DI PIU LIBRERIE CON LO STESSO NOME POTREBBE CAUSARE PROBLEMI
#include <Servo.h>                //https://github.com/jkb-git/ESP32Servo
#include "Arduino.h"
#include "Audio.h"                //https://github.com/schreibfaul1/ESP32-audioI2S            
#include "SPI.h"                    
#include "SD.h"
#include "FS.h"
//per web server
#include <WiFi.h>
#include <AsyncTCP.h>             //https://github.com/me-no-dev/AsyncTCP
#include <ESPAsyncWebServer.h>    //https://github.com/me-no-dev/ESPAsyncWebServer
#include <WiFiAP.h>
#include <HTTPClient.h>

//Digital I/O used
#define SD_CS          5
#define SPI_MOSI      23
#define SPI_MISO      19
#define SPI_SCK       18
#define I2S_DOUT      25
#define I2S_BCLK      27
#define I2S_LRC       26

Audio audio;
static const char fileAudio[] = "/audio.mp3";
static const char fileMovimenti[] = "/servo.txt";   //lo slash è obbligatorio
static const int servoPin = 4;
static uint8_t buf[512];  //buffer dove viene memorizzato il contenuto del file
static int puls = 34;     //pin pulsante
static int laser = 2;     //pin laser
Servo servo1;
bool attesaRestart = false; 
bool setServo = true;  //se non si è premuto il pulsante, permette di settare le posizioni del servo e memorizzare in file tramite web server
bool primoStart = true;
int len = 0;          //lunghezza del file servo
int angoloMem = 0;
int posInit = 25;     //posizione iniziale
String laserOn = "";  //se "1" vuol dire che il laser, durante il movimento rimane acceso e il posizionamento del laser avviene lentamente


//il form NON ha i controlli sui valori inseriti in input
//se si inserisce l'opzione "laser acceso" il movimento avviene lentamente, altrimenti avviene velocemente
String FORMservo = "<form method=\"POST\" action= \"/rispFORMservo\"><br><H2>MUSEUM</H2><br><br>angolo<input type=\"text\" name=\"ang\"><br>deltaT<input type=\"text\" name=\"delt\"><br>laser acceso<input type=\"radio\" name=\"las\" value=\"lasOn\"><br>acquisisci in file<input type=\"radio\" name=\"fil\" value=\"inFile\"><br><input type=\"submit\" value=\"CONTINUA\"></H2></form> <form method=\"POST\" action= \"/eliminaFile\"><br><input type=\"submit\" value=\"ELIMINA FILE POSIZIONI\"></form>";
String FORMciclo;
String formCiclo1 = "<form method=\"POST\" action= \"/rispFORMservo\"><br><H2>Valore angolo attuale: ";
String formCiclo2 = "<br><br>nuovo angolo<input type=\"text\" name=\"ang\"><br>deltaT<input type=\"text\" name=\"delt\"><br>laser acceso<input type=\"radio\" name=\"las\" value=\"lasOn\"><br>acquisisci in file<input type=\"radio\" name=\"fil\" value=\"inFile\"><br><input type=\"submit\" value=\"CONTINUA\"></H2></form> <form method=\"POST\" action= \"/eliminaFile\"><br><input type=\"submit\" value=\"ELIMINA FILE POSIZIONI\"></form>";
String eliminaFile = (String)"<h1>File delle posizioni " + fileMovimenti + (String)" eliminato</h1><br><br><form method=\"POST\" action=\"/rispFORMservo\"><input type=\"submit\" value=\"CONTINUA\">";

                                           
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

//CREAZIONE TASK voice E servo ESEGUITI CONTEMPORANEAMENTE
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
                    NULL);            /* Task handle. */

//CREAZIONE PAGINE DEL WEB SERVER
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
  {
    request->send(200, "text/html", FORMservo);
  }); 
 //acquisisce i dati inviati dal form su browser    
  server.on("/rispFORMservo", HTTP_POST, [](AsyncWebServerRequest *request)
  { 
    int angolo = -1; //se non si inserisce il campo nel form il servo non si deve muovere
    Servo servo2;
    servo2.attach(servoPin);
    String angoloStr, deltaT, laserOn = "0";
    int paramsNr = request->params();
    Serial.println(paramsNr);
    for(int i=0;i<paramsNr;i++)
    { 
        AsyncWebParameter* p = request->getParam(i);

        if (p->name()=="ang")    
        {
            angolo = p->value().toInt();
            angoloStr = p->value();
            Serial.print("angolo: ");  Serial.println(angolo);  
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
               appendFile(SD, fileMovimenti, message);  //per fare eseguire i nuovi movimenti inseriti nel file, bisogna riavviare l'esp
           }
        }
    }
    
    FORMciclo = formCiclo1 + angoloStr + formCiclo2;

    request->send(200, "text/html", FORMciclo); 

    //esegue il movimento in base ai parametri inseriti nel form
    if(angolo != -1)
    {
      digitalWrite(laser, LOW);
      servo2.write(angolo);
      delay(500);
      digitalWrite(laser, HIGH); 
    }
  }); 

  server.on("/eliminaFile", HTTP_POST, [](AsyncWebServerRequest *request)
  {
    File fileChiudi = SD.open(fileMovimenti, FILE_WRITE);
    fileChiudi.close();
    request->send(200, "text/html", eliminaFile);
  }); 
            
  server.begin();
  Serial.println("Server started");
//posiziona il servo al centro
  servo1.write(posInit);  
}

void loop() {
//se la funzione loop è vuota, può verificarsi un errore
  vTaskDelay(10);
}

void voice( void * parameter )
{
 //attesaRestart si setta dopo che l'audio è stato riprodotto completamente
    for(;;){    //loop
     if(attesaRestart){ while(true)
            { delay(1000);  //altrimenti va in WD e riparte
             if (digitalRead(puls) == HIGH) ESP.restart();
            }
           }
    if (primoStart && digitalRead(puls) == HIGH) 
       {audio.connecttoSD(fileAudio);    Serial.println("pulsante premuto");
        primoStart = false;
        Serial.print("GRANDEZZA FILE: ");
        Serial.println(audio.getFileSize());
       }
    else if (primoStart) 
    {
      delay(500);  
    }

    if(!attesaRestart && !primoStart)
    {
      audio.loop();
      Serial.print("SECONDI: ");
      Serial.println(audio.getAudioCurrentTime());
      Serial.print("POSIZIONE: ");
      Serial.println(audio.getFilePos());
      Serial.print("GRANDEZZA FILE: ");
      Serial.println(audio.getFileSize());

      if (audio.getFilePos() == audio.getFileSize() || audio.getFilePos() > audio.getFileSize())    
      {
        audio.stopSong();
        attesaRestart = true;
        digitalWrite(laser, LOW);
      }
    }
  }
}
 
void servo( void * parameter)
{
  for (;;)      //loop
  { 
     if(setServo && digitalRead(puls) == HIGH)
     {digitalWrite(laser, LOW);
      servo1.write(posInit);
      setServo = false;  Serial.println("pulsante premuto2");
      int i = 0;
      String riga;
      readFile(SD, fileMovimenti);  //mette nella variabile len la lunghezza del file e in buf il contenuto
      int posMem = posInit;       //posizione precedente (per capire in quale verso effettuare il movimento graduale)
     while (i < len)              //ciclo che legge tutto il file
      { riga = ""; 
       while (buf[i] != 0xD)      //legge una riga dal file, formato:      TEMPO;POSIZIONE;LASER\n
        {  riga += (char)buf[i];
           i++;
        }   
       Serial.println(riga); 

       int posizione = 0;
       String sTempo = "", sPos = "", sMoving = "";

       while(riga[posizione] != ';')
       {
          sTempo += riga[posizione];
          posizione++;
       }

       posizione++;

       while(riga[posizione] != ';')
       {
          sPos += riga[posizione];
          posizione++;
       }

       posizione++;
       
       int tempo = sTempo.toInt(); // tempo prima di posizionarsi
       int pos = sPos.toInt(); // posizione in gradi
       char moving = riga[posizione]; //acceso = '1' e movimento lento, spento  = '0' e movimento veloce

       Serial.println(tempo);
       Serial.println(pos);
       Serial.println(moving);
       delay(tempo*1000);       //in ms,     attende prima di effettuare il movimento


//0 --> 180: giù
//180 ---> 0: su
//!!!può cambiare in base al motore utilizzato o al verso (testato su MG996R)

       if (moving == '0')          //se il parametro è 0, il movimento avviene rapidamente e a laser spento
         {   
             digitalWrite(laser, LOW);
             servo1.write(pos);
             delay(600);  //tempo di posizionamento
             digitalWrite(laser, HIGH);
         }
       if (moving == '1')           //se il parametro è 1, il movimento avviene lentamente e a laser acceso
         {   
             digitalWrite(laser, HIGH);

             if(pos > posMem)         //movimento verso il basso
             {
                for (int j = posMem; j < pos; j=j+2)
                {
                  servo1.write(j);
                  delay(100);  //tempo di posizionamento
                }
             }
             else if(pos < posMem)     //movimento verso l'alto
             {
                for (int j = posMem; j > pos; j=j-2)
                {
                  servo1.write(j);
                  delay(100);  //tempo di posizionamento
                }
             }
         }
       i += 2;  //come fine riga c'è 0xD 0xA
       posMem = pos;
      }
     }
   delay(500);
   //fine lettura dei movimenti dal file
  }
}


//funzioni per lettura e scrittura file
void readFile(fs::FS &fs, const char * path){
    Serial.printf("Reading file: %s\n", path);

    File file = fs.open(path);
    if(!file){
        Serial.println("Failed to open file for reading");
        return;
    }
      len = file.size();
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
    file.close();
}
