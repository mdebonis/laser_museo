# TECA MULTIMEDIALE

> Realizzazione di una teca multimediale (movimento di un laser che punta a degli oggetti da descrivere e riproduzione audio
contenente la descrizione dell'oggetto puntato dal laser), con il
microcontrollore ESP-32, in linguaggio C++.

# FUNZIONANTE / DA COMPLETARE

Il codice ([teca_multimediale_v8.ino](https://github.com/mdebonis/laser_museo/blob/main/teca_multimediale_v8/teca_multimediale_v8.ino)) e il circuito sono sono funzionanti e permettono il movimento di un solo
servomotore, quindi il laser può muoversi lungo un solo asse, inoltre il circuito
è interamente alimentato dal cavo USB utilizzato in fase di realizzazione per caricare
il codice nella memoria dell'ESP-32. Da implementare la gestione di due servomotori
e la creazione di una fonte di alimentazione fissa per il completamento del progetto.

La versione successiva del codice ([teca_multimediale_v9.ino](https://github.com/mdebonis/laser_museo/blob/main/teca_multimediale_v9/teca_multimediale_v9.ino)) permette la gestione di due servomotori e quindi il movimento del laser in due dimensioni. **Non è ancora stato testato, quindi potrebbe non funzionare**. **Lo [schema](#Schema) sottostante funziona solamente con il codice [teca_multimediale_v8.ino](https://github.com/mdebonis/laser_museo/blob/main/teca_multimediale_v8/teca_multimediale_v8.ino)**

# Schema

![schema](https://github.com/mdebonis/laser_museo/blob/main/images/schema.png)

# Come utilizzare

Una volta avviato l'ESP-32, esso è in **modalità server web**, quindi è possibile trovare
la pagina web inserendo nel browser l'indirizzo **192.168.4.1**, dopo essersi collegati
all'access point con il nome e la password specificate nel codice.
La pagina web contiene un form tramite il quale è possibile impostare i parametri per il
movimento del laser e **provare il movimento**. Se il movimento che si è appena provato
è giusto, è possibile spuntare un'opzione del form che permette di **salvarlo in un file**.

Quando si preme il pulsante, l'ESP-32 inizia la **riproduzione del file audio** ed
**effettua i movimenti** registrati nel file descritto in precedenza, quindi entra in
**modalità esposizione**. Una volta terminata la riproduzione del file audio, se si
preme il pulsante l'ESP-32 si riavvia e ritorna in modalità server web.

# Come installare

L'installazione del progetto richiede la realizzazione del circuito secondo lo
[schema](#Schema) descritto e l'installazione dell'ambiente di sviluppo
con le annesse librerie, come descritto nella [documentazione](#Documentazione).

# Documentazione

## Codice

Il codice caricato nell'ESP-32 è scritto in C++ ed è composto da due task principali
che vengono eseguiti contemporaneamente (Task riproduzione audio, Task movimento servomotore),
oltre alla sezione di setup del web server e delle varie interfacce e pin.

Per una descrizione dettagliata del codice, [si rimanda al codice stesso commentato.](https://github.com/mdebonis/laser_museo/blob/main/teca_multimediale_v8/teca_multimediale_v8.ino)

## Ambiente di sviluppo

### Arduino IDE

L'ambiente di sviluppo utilizzato è **Arduino IDE** che, se configurato correttamente,
permette di scrivere codice, caricarlo e installare librerie per il microcontrollore ESP-32.

[Link al tutorial per la configurazione completa dell' Arduino IDE](https://randomnerdtutorials.com/installing-the-esp32-board-in-arduino-ide-windows-instructions/)

### Librerie

```C++
#include <Servo.h>                //https://github.com/jkb-git/ESP32Servo
#include "Arduino.h"
#include "Audio.h"                //https://github.com/schreibfaul1/ESP32-audioI2S            
#include "SPI.h"                    
#include "SD.h"
#include "FS.h"
#include <WiFi.h>
#include <AsyncTCP.h>             //https://github.com/me-no-dev/AsyncTCP
#include <ESPAsyncWebServer.h>    //https://github.com/me-no-dev/ESPAsyncWebServer
#include <WiFiAP.h>
#include <HTTPClient.h>
```

> POTREBBE ESSERE NECESSARIO RIMUOVERE ALCUNE LIBRERIE STANDARD DI ARDUINO CHE HANNO LO STESSO NOME DELLE NUOVE LIBRERIE DA IMPORTARE.
LA PRESENZA DI PIU' LIBRERIE CON LO STESSO NOME POTREBBE CAUSARE PROBLEMI.

Link per l'installazione delle librerie:
- [<Servo.h>](https://github.com/jkb-git/ESP32Servo)
- ["Audio.h"](https://github.com/schreibfaul1/ESP32-audioI2S)
- [<AsyncTCP.h>](https://github.com/me-no-dev/AsyncTCP)
- [<ESPAsyncWebServer.h>](https://github.com/me-no-dev/ESPAsyncWebServer)

> Le librerie per le quali non è presente il link sono già preinstallate nell'Arduino IDE,
tuttavia potrebbero non funzionare per ESP-32: in caso di errori, è necessario cercarle sul web per installarle.

## Componenti

### ESP-32

ESP32 è una serie di microcontrollore system-on-a-chip a basso costo e a basso consumo. Ha un wi-fi integrato e radio Bluetooth dual-mode. Ha molte delle capacità dell'Arduino e può essere programmato con il software Arduino IDE.
la ESP32 possiede un chip Tensilica Xtensa dual-core LX6, basato su architettura a 32BIT, in grado di raggiungere i 240MHz e con ben 4MB di memoria flash, associati a 512KB di RAM. Possiamo adoperare ben 36 PIN GPIO tutti PWM, lavorando con una tensione a 3,3V, senza però dimenticare che possiamo alimentare questa scheda anche con i 5V dal connettore Micro-USB o da pin VIN.

### Servomotore MG996R

Il Servomotore MG996R Digital è dotato di ingranaggi in metallo che permettono di raggiungere una coppia di 9,4 kg con una alimentazione a 4,8Vcc e di 11 Kg con una alimentazione a 6Vcc.
L'unità è fornita completa di un cavo di connessione da 30 cm terminato con un connettore femmina a 3 poli. È collegato all’esp-32 tramite un semplice pin digitale.
Il Servomotore MG996R può ruotare di circa 180° ed è fornito con una ampia gamma di accessori, tra i quali i supporti utilizzati nel progetto per far muovere il laser e per permettere di collegare insieme due servomotori.

### Diodo laser

Il componente principale è rappresentato da un diodo laser che emette un fascio laser di colore rosso a bassa potenza.
I suo assorbimento è di soli 30 mA, per cui in linea con la massima la corrente nominale che un pin digitale di Arduino può fornire pari a 40 mA.
Come il servomotore, il laser è collegato ad un pin digitale dell’esp-32.

### D/A audio converter PCM1502A

Dispositivo che include un convertitore digitale-analogico audio e circuiti di supporto aggiuntivi, grazie all’architettura d’ultima generazione si riescono ad ottenere eccellenti prestazione dinamiche. Supporta carichi fino a 1kΩ per pin e il PLL integrato ci permette di connetterlo tramite bus I2S all’ESP32.

### Micro-SD-card reader

Il micro-SD-reader permette di leggere la scheda SD con gli audio utilizzati per il progetto. I moduli della scheda SD sono collegati tramite SPI al microcontroller ESP32. La comunicazione SPI garantisce che i dati siano trasferiti il più velocemente possibile. Il microcontroller ESP32 e il modulo scheda SD hanno la stessa tensione operativa di 3,3V. Se si alimenta la scheda ESP32 tramite Micro-USB, è anche possibile utilizzare il pin v5 che fornisce un alimentatore di 5V per la scheda SD. Il regolatore di tensione interno e lo shifter a livello logico riducono la tensione di alimentazione fino a 3,3V e traducono anche la comunicazione SPI al livello di tensione desiderato.

### Condensatore elettrolitico (47mf)

Utilizzato nel progetto per abilitare permanentemente la modalità flash/upload sull’esp-32.
Ha un perno positivo (l'anodo) ed un perno negativo chiamato catodo. Quando la tensione è applicata ad un condensatore elettrolitico, l'anodo deve essere ad una tensione superiore al catodo. Il catodo di un condensatore elettrolitico viene generalmente identificato con marchiato un '-' , e una striscia colorata sul case. La gamba dell'anodo potrebbe anche essere leggermente più lunga. Quando la corrente fluisce in un condensatore, le cariche sono "bloccate" sulle piastre perché non possono andare oltre il dielettrico isolante. Gli elettroni (particelle con carica negativa) vengono risucchiati in una delle piastre, diventando così di carica negativa. La grande massa di cariche negative su una piastra spinge via le altre cariche sull'altra piastra, rendendola carica positivamente. Le cariche positive e negative su ciascuno di queste piastre si attraggono, perché è quello che fanno cariche le opposte. Ma, con il dielettrico tra di loro, per quanto vogliono unirsi, le cariche saranno sempre bloccate sulla piastra (fino a quando non avranno un altro posto dove andare). Le cariche stazionarie su queste piastre creano un campo elettrico, che influenzano energia potenziale elettrica e tensione. Quando un gruppo di cariche si trovano su un condensatore di questo tipo, il condensatore può immagazzinare energia elettrica, come una batteria può immagazzinare energia chimica.

### Resistenza (10kΩ)

### Pulsante
