/*  versione 10: implementazione movimento di tre servomotori
 *  versione 9: implementazione movimento di due servomotori, possibilità di riavviare l'esp dal web, pulsante di avviamento tramite web
 *  versione 8: correzione bug, interruzione audio automatica, opzione eliminazione file movimenti da web
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
 *  Il formato del file di testo sarà il seguente: tempo di ritardo prima di compiere il movimento, posizione x e y
 *  in coordinate cartesiane, laser acceso o spento durante il percorso. p.es:
 *  3;120;90;1   aspetta 3 sec. poi si posiziona su x=120, y=90 con laser acceso con campi a lunghezza variabile.
 *  Il programma legge una riga, effettua il movimento e poi legge la successiva
 */


// CONTROLLA DIMENSIONE DI buf[512]
// PULSANTE DI AVVIO


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
static const char filePosizioneServo[] = "/posizione.txt";
static const int servoPinY = 4;
static const int servoPinX = 13;
static const int servoPinZ = 32;
static uint8_t buf[512];  //buffer dove viene memorizzato il contenuto del file
static int laser = 2;     //pin laser
Servo servoY;
Servo servoX;
Servo servoZ;    // per il movimento della scheda madre
bool attesaRestart = false;
bool setServo = true;  //se non si è premuto il pulsante, permette di settare le posizioni del servo e memorizzare in file tramite web server
bool primoStart = true;
bool pulsantePremuto = false;
int len = 0;          //lunghezza del file servo
int angoloMem = 0;
int posInitY = 100;     //posizione iniziale
int posInitX = 80;
int posInitZ = 102;
String laserOn = "";  //se "1" vuol dire che il laser, durante il movimento rimane acceso e il posizionamento del laser avviene lentamente


//il form NON ha i controlli sui valori inseriti in input
//se si inserisce l'opzione "laser acceso" il movimento avviene lentamente, altrimenti avviene velocemente
String inizio = "<head> <style> input {border-radius: 30px; font-size: 3em; background-color: #4CAF50;  border: none;  color: white;  padding: 16px 32px;  text-decoration: none;  margin: 4px 2px;  cursor: pointer; position: fixed;  top: 50%;  left: 50%;  transform: translate(-50%, -50%); width: 70%; height: 70%}</style></head><form method=\"POST\" action= \"/avvia\"><br><input type=\"submit\" value=\"AVVIA\"></form>";
String avviato = "<head> <style> input {font-size: 3em; border: none;  padding: 16px 32px;  text-decoration: none;  margin: 4px 2px; position: fixed;  top: 50%;  left: 50%;  transform: translate(-50%, -50%); width: 70%; height: 70%}</style></head><p>ESPOSIZIONE AVVIATA,<br><br>BUON<br>DIVERTIMENTO!</p>";
String FORMservo = "<form method=\"POST\" action= \"/rispFORMservo\"><br><H2>MUSEUM</H2><br><br>angoloY<input type=\"text\" name=\"angY\"><br>angoloX<input type=\"text\" name=\"angX\"><br>angoloZ<input type=\"text\" name=\"angZ\"><br>deltaT<input type=\"text\" name=\"delt\"><br>laser acceso<input type=\"radio\" name=\"las\" value=\"lasOn\"><br>acquisisci in file<input type=\"radio\" name=\"fil\" value=\"inFile\"><br><input type=\"submit\" value=\"CONTINUA\"></H2></form> <form method=\"POST\" action= \"/riavviaEsp\"><br><input type=\"submit\" value=\"SALVA I NUOVI MOVIMENTI\"></form> <form method=\"POST\" action= \"/eliminaFile\"><br><input type=\"submit\" value=\"ELIMINA FILE POSIZIONI\"></form>";
String FORMciclo;
String formCiclo1 = "<form method=\"POST\" action= \"/rispFORMservo\"><br><H2>Valori angoli attuali: ";
String formCiclo2 = "<br><br>nuovo angolo Y<input type=\"text\" name=\"angY\"><br>nuovo angolo X<input type=\"text\" name=\"angX\"><br>nuovo angoloZ<input type=\"text\" name=\"angZ\"><br>deltaT<input type=\"text\" name=\"delt\"><br>laser acceso<input type=\"radio\" name=\"las\" value=\"lasOn\"><br>acquisisci in file<input type=\"radio\" name=\"fil\" value=\"inFile\"><br><input type=\"submit\" value=\"CONTINUA\"></H2></form> <form method=\"POST\" action= \"/riavviaEsp\"><br><input type=\"submit\" value=\"SALVA I NUOVI MOVIMENTI\"></form> <form method=\"POST\" action= \"/eliminaFile\"><br><input type=\"submit\" value=\"ELIMINA FILE POSIZIONI\"></form> ";
String eliminaFile = (String)"<h1>File delle posizioni " + fileMovimenti + (String)" eliminato</h1><br><br><form method=\"POST\" action=\"/rispFORMservo\"><input type=\"submit\" value=\"CONTINUA\">";
String riavvia = "Esp riavviato, ora il file è aggiornato con le nouve posizioni inserite. Bisogna riconnettersi all'Access Point";


AsyncWebServer server(80);
const char *ssid = "esp32museum";
const char *password = "museum57";

void setup() {
    Serial.begin(115200);
    pinMode(SD_CS, OUTPUT);
    pinMode(laser, OUTPUT);
    digitalWrite(laser, LOW);
    digitalWrite(SD_CS, HIGH);
    SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI);
    SPI.setFrequency(1000000);
    if(SD.begin(SD_CS))
    {
      Serial.println("Apertura scheda SD riuscita");
    }
    else
    {
      Serial.println("Apertura scheda SD fallita");
    }
    /*servoY.attach(servoPinY);
    servoX.attach(servoPinX);
    servoZ.attach(servoPinZ);*/
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
    request->send(200, "text/html", inizio);
  });

  server.on("/avvia", HTTP_POST, [](AsyncWebServerRequest *request)
  {
    pulsantePremuto = true;
    request->send(200, "text/html", avviato);
  });

  server.on("/impostamovimenti", HTTP_GET, [](AsyncWebServerRequest *request)
  {
    request->send(200, "text/html", FORMservo);
  });
  
 //acquisisce i dati inviati dal form su browser
  server.on("/rispFORMservo", HTTP_POST, [](AsyncWebServerRequest *request)
  {
    int angoloY = -1; //se non si inserisce il campo nel form il servo non si deve muovere
    int angoloX = -1;
    int angoloZ = -1;
    Servo servoY_2;
    Servo servoX_2;
    Servo servoZ_2;
    servoY_2.attach(servoPinY);
    servoX_2.attach(servoPinX);
    servoZ_2.attach(servoPinZ);
    String angoloStrY, angoloStrX, angoloStrZ, deltaT, laserOn = "0";
    int paramsNr = request->params();
    Serial.println(paramsNr);
    for(int i=0;i<paramsNr;i++)
    {
        AsyncWebParameter* p = request->getParam(i);

        if (p->name()=="angY")
        {
            angoloY = p->value().toInt();
            angoloStrY = p->value();
            Serial.print("angolo Y: ");  Serial.println(angoloY);
        }
        if (p->name()=="angX")
        {
            angoloX = p->value().toInt();
            angoloStrX = p->value();
            Serial.print("angolo X: ");  Serial.println(angoloX);
        }
        if (p->name()=="angZ")
        {
            angoloZ = p->value().toInt();
            angoloStrZ = p->value();
            Serial.print("angolo Z: ");  Serial.println(angoloZ);
        }
        if (p->name()=="delt")
           {deltaT = p->value();}  //deltaT è un stringa dovendolo inserire nel file
        if (p->name()=="las")
           {if (p->value()=="lasOn") laserOn = "1"; }
        if (p->name()=="fil")
           {if (p->value()=="inFile")
              { Serial.println(angoloStrX); Serial.println(angoloStrY); Serial.println(angoloStrZ); Serial.println(laserOn);
                String s = ""; Serial.println(s);
                s += deltaT;  Serial.println(s);
                s += ";";      Serial.println(s);
                s += angoloStrX;  Serial.println(s);
                s += ";";        Serial.println(s);
                s += angoloStrY;  Serial.println(s);
                s += ";";        Serial.println(s);
                s += angoloStrZ;  Serial.println(s);
                s += ";";        Serial.println(s);
                s += laserOn;    Serial.println(s);
               char message[18];                                              Serial.println("x");
               int lung = s.length();
               for (int i=0; i< lung;i++) message[i] = s[i];
               message[lung] = 0xD;
               message[lung+1] = 0xA;
               message[lung+2] = '\0';
               appendFile(SD, fileMovimenti, message);  //per fare eseguire i nuovi movimenti inseriti nel file, bisogna riavviare l'esp
           }
        }
    }

    FORMciclo = formCiclo1 + "x=" + angoloStrX + ", y=" + angoloStrY + ", z=" + angoloStrZ + formCiclo2;

    request->send(200, "text/html", FORMciclo);

    //esegue il movimento in base ai parametri inseriti nel form
    if(angoloX != -1 && angoloY != -1 && angoloZ != -1)
    {
      digitalWrite(laser, LOW);
      servoY_2.write(angoloY);
      servoX_2.write(angoloX);
      servoZ_2.write(angoloZ);
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

  server.on("/riavviaEsp", HTTP_POST, [](AsyncWebServerRequest *request)
  {
    request->send(200, "text/html", riavvia);
    ESP.restart();
  });

  server.begin();
  Serial.println("Server started");
//posiziona il servo al centro

    servoY.attach(servoPinY);
    servoX.attach(servoPinX);
    servoZ.attach(servoPinZ);
    
  servoY.write(posInitY);
  servoX.write(posInitX);
  servoZ.write(posInitZ);

  delay(1000);

  servoY.detach();
  servoX.detach();
  servoZ.detach();
}

void loop() {
//se la funzione loop è vuota, può verificarsi un errore
  vTaskDelay(10);
}

void voice( void * parameter )
{
 //attesaRestart si setta dopo che l'audio è stato riprodotto completamente
    for(;;){    //loop
     if(attesaRestart){/* while(true)
            { delay(1000);  //altrimenti va in WD e riparte
             if (digitalRead(puls) == HIGH) ESP.restart();
            }*/
            delay(2000);
            ESP.restart();
           }
    if (primoStart && pulsantePremuto)
       {
        //pulsantePremuto = false;
        
        audio.connecttoSD(fileAudio);    Serial.println("pulsante premuto");
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

      if(audio.getFileSize() == 0)
      {
        audio.stopSong();
        attesaRestart = true;
        Serial.print("AUDIO INTERROTTO");
        digitalWrite(laser, LOW);
      }
    }
  }
}

void servo( void * parameter)
{
  for (;;)      //loop
  {
     if(setServo && pulsantePremuto)
     {
      //pulsantePremuto = false;
      
      digitalWrite(laser, LOW);
      servoY.attach(servoPinY);
      servoX.attach(servoPinX);
      servoZ.attach(servoPinZ);
      
      servoY.write(posInitY);
      servoX.write(posInitX);
      servoZ.write(posInitZ);

      servoY.detach();
      servoX.detach();
      servoZ.detach();
      setServo = false;  Serial.println("pulsante premuto2");
      int i = 0;
      String riga;
      readFile(SD, fileMovimenti);  //mette nella variabile len la lunghezza del file e in buf il contenuto
      int posMemY = posInitY;       //posizione precedente (per capire in quale verso effettuare il movimento graduale)
      int posMemX = posInitX;
      int posMemZ = posInitZ;
     while (i < len)              //ciclo che legge tutto il file
      { riga = "";
       while (buf[i] != 0xD)      //legge una riga dal file, formato:      TEMPO;POSIZIONEX;POSIZIONEY;LASER\n
        {  riga += (char)buf[i];
           i++;
        }
       Serial.println(riga);

       int posizione = 0;
       String sTempo = "", sPosX = "", sPosY = "", sPosZ = "", sMoving = "";

       while(riga[posizione] != ';')
       {
          sTempo += riga[posizione];
          posizione++;
       }

       posizione++;

       while(riga[posizione] != ';')
       {
          sPosX += riga[posizione];
          posizione++;
       }

       posizione++;

       while(riga[posizione] != ';')
       {
          sPosY += riga[posizione];
          posizione++;
       }

       posizione++;

       while(riga[posizione] != ';')
       {
          sPosZ += riga[posizione];
          posizione++;
       }

       posizione++;

       int tempo = sTempo.toInt(); // tempo prima di posizionarsi
       int posX = sPosX.toInt(); // posizione in gradi
       int posY = sPosY.toInt();
       int posZ = sPosZ.toInt();
       char moving = riga[posizione]; //acceso = '1' e movimento lento, spento  = '0' e movimento veloce

       Serial.println(tempo);
       Serial.println(posX);
       Serial.println(posY);
       Serial.println(posZ);
       Serial.println(moving);
       delay(tempo*1000);       //in ms,     attende prima di effettuare il movimento


//0 --> 180: giù
//180 ---> 0: su
//!!!può cambiare in base al motore utilizzato o al verso (testato su MG996R)

        //GETIONE MOVIMENTO SCHEDA MADRE A PARTE, PERCHE DEVE ESSERE SEMPRE LENTO

        servoY.attach(servoPinY);
        servoX.attach(servoPinX);
        servoZ.attach(servoPinZ);

        if(posZ > posMemZ)         //movimento verso il basso
        {
           for (int j = posMemZ; j < posZ; j=j+2)
           {
             servoZ.write(j);
             delay(100);  //tempo di posizionamento
           }
        }
        else if(posZ < posMemZ)     //movimento verso l'alto
        {
           for (int j = posMemZ; j > posZ; j=j-2)
           {
             servoZ.write(j);
             delay(100);  //tempo di posizionamento
           }
        }

        //--------------------------------------

       if (moving == '0')          //se il parametro è 0, il movimento avviene rapidamente e a laser spento
         {
             digitalWrite(laser, LOW);
             servoY.write(posY);
             servoX.write(posX);
             delay(600);  //tempo di posizionamento
             digitalWrite(laser, HIGH);
         }
       if (moving == '1')           //se il parametro è 1, il movimento avviene lentamente e a laser acceso
         {
             digitalWrite(laser, HIGH);


             //IL SEGUENTE CODICE COMMENTATO EFFETTUA PRIMA IL MOVIMENTO DEL SERVO Y E POI QUELLO DEL SERVO X

             /*if(posY > posMemY)         //movimento verso il basso
             {
                for (int j = posMemY; j < posY; j=j+2)
                {
                  servoY.write(j);
                  delay(100);  //tempo di posizionamento
                }
             }
             else if(posY < posMemY)     //movimento verso l'alto
             {
                for (int j = posMemY; j > posY; j=j-2)
                {
                  servoY.write(j);
                  delay(100);  //tempo di posizionamento
                }
             }

             if(posX > posMemX)         //movimento verso il basso
             {
                for (int j = posMemX; j < posX; j=j+2)
                {
                  servoX.write(j);
                  delay(100);  //tempo di posizionamento
                }
             }
             else if(posX < posMemX)     //movimento verso l'alto
             {
                for (int j = posMemX; j > posX; j=j-2)
                {
                  servoX.write(j);
                  delay(100);  //tempo di posizionamento
                }
             }*/

             //IL SEGUENTE CODICE EFFETTUA I MOVIMENTI DEI DUE SERVO CONTEMPORANEAMENTE, IN DIAGONALE

             int posizioniX[200], posizioniY[200];
             int nPosX = 0, nPosY = 0;

             //CARICAMENTO ARRAY DELLE POSIZIONI

             if(posY > posMemY)         //movimento verso il basso
             {
                for (int j = posMemY; j < posY; j=j+2)
                {
                  posizioniY[nPosY] = j;
                  nPosY++;
                }
             }
             else if(posY < posMemY)     //movimento verso l'alto
             {
                for (int j = posMemY; j > posY; j=j-2)
                {
                  posizioniY[nPosY] = j;
                  nPosY++;
                }
             }

             if(posX > posMemX)         //movimento verso il basso
             {
                for (int j = posMemX; j < posX; j=j+2)
                {
                  posizioniX[nPosX] = j;
                  nPosX++;
                }
             }
             else if(posX < posMemX)     //movimento verso l'alto
             {
                for (int j = posMemX; j > posX; j=j-2)
                {
                  posizioniX[nPosX] = j;
                  nPosX++;
                }
             }

             //ESECUZIONE MOVIMENTI

             if(nPosX >= nPosY)
             {
               for(int i=0; i<nPosX; i++)
               {
                 servoX.write(posizioniX[i]);

                 if(i < nPosY)
                    servoY.write(posizioniY[i]);

                 delay(100);
               }
             }
             else
             {
               for(int i=0; i<nPosY; i++)
               {
                 servoY.write(posizioniY[i]);

                 if(i < nPosX)
                    servoX.write(posizioniX[i]);

                 delay(100);
               }
             }
         }

       servoY.detach();
        servoX.detach();
        servoZ.detach();
         
       i += 2;  //come fine riga c'è 0xD 0xA
       posMemX = posX;
       posMemY = posY;
       posMemZ = posZ;
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

/*void writeFile(fs::FS &fs, const char * path, const char * message){
    Serial.printf("Writing to file: %s\r\n", path);

    File file = fs.open(path, FILE_WRITE);
    if(!file){
        Serial.println("- failed to open file for writing");
        return;
    }
    if(file.print(message)){
        Serial.println("- message written");
    } else {
        Serial.println("- writing failed");
    }
    file.close();
}*/

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
