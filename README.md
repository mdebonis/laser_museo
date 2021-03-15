# TECA MULTIMEDIALE

> Realizzazione di una teca multimediale (movimento di un laser, riproduzione audio) con il
microcontrollore ESP-32, in linguaggio C++.

# FUNZIONANTE / DA COMPLETARE

Il codice e il circuito sono sono funzionanti e permettono il movimento di un solo
servomotore, quindi il laser può muoversi lungo un solo asse, inoltre il circuito
è interamente alimentato dal cavo USB utilizzato in fase di realizzazione per caricare
il codice nella memoria dell'ESP-32. Da implementare la gestione di due servomotori
e la creazione di una fonte di alimentazione fissa per il completamento del progetto.

## Immagini e schema

![schema](https://github.com/tonygiuliani/laser_museo/blob/main/images/schema.jpeg)

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
[schema](## Immagini e schema) descritto e l'installazione dell'ambiente di sviluppo
con le annesse librerie, come descritto nella [documentazione](# Documentazione).

# Documentazione



## Link a documentazione esterna



# Licenza generale

## Autori e Copyright
