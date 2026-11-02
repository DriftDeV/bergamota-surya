# Dual boot Android + iOS sul Poco X3 NFC (idea futura)

Stato: **idea, nulla eseguito.** Scritto il 2026-11-02 dopo la richiesta dell'utente: Android e iOS sullo stesso telefono, circa 64 GB ciascuno.
Distinzione usata sotto: **[fatto]** misurato o letto nel repo, **[ipotesi]** ragionamento non verificato.

## 1. Fattibilità in breve

Fattibile, con lavoro medio. Tre pezzi:

| Pezzo | Difficoltà | Stato |
|---|---|---|
| Dividere lo spazio in due partizioni | media, tocca la GPT (richiede un OK esplicito per ogni scrittura) | non iniziato |
| Far partire iOS dalla sua partizione | bassa | oggi iOS usa tutta `userdata` |
| Far partire Android quando lo si sceglie | **la parte incerta** | Enxerto è in `boot`, Android non parte da solo |

## 2. Spazio: 64 + 64 non entrano, anche ridimensionando le altre partizioni

Letto dalla GPT del LUN 0 (`backup/surya/lun-sda-head1M.img`, settore da 4096 byte, 16 partizioni) [fatto]:

| Partizione | Dimensione | Nota |
|---|---|---|
| `userdata` | 104,83 GiB | dati Android, oggi radice iOS |
| `super` | 8,00 GiB | sistema Android (system, vendor, product): serve ad Android |
| `cust` | 1,00 GiB | Xiaomi |
| `cache` | 0,38 GiB | |
| `exaid` | 0,38 GiB | |
| `recovery` | 0,125 GiB | OrangeFox, da tenere |
| `minidump` | 0,125 GiB | |
| altre 8 (`persist`, `misc`, `keystore`, `frp`, `vbmeta_system`, `metadata`, `gsort`, `ffu`) | circa 0,1 GiB in tutto | |
| **Totale LUN 0** | **114,90 GiB** | tutto lo spazio utilizzabile del LUN |

- Il disco è il modello da 128 GB: 114,9 GiB sul LUN 0. Gli altri LUN (sdb-sdf: bootloader, modem, firmware) sono piccoli e non riutilizzabili [fatto].
- Hai ragione che `userdata` è solo una parte, ma il resto del LUN 0 vale circa **10 GiB**, e quasi tutto è `super`, che Android richiede. Ridimensionare le altre partizioni recupera al massimo 1-2 GiB (`cache`, `exaid`, `minidump`, parte di `cust`).
- Quindi **64 GiB per iOS** lascia ad Android **circa 51 GiB in tutto**: 8 GiB di `super`, 1 GiB di `cust`, il resto piccolo e circa **40 GiB di dati**. Una divisione alla pari sarebbe circa 55 GiB per parte.
- Con "64 GB" decimali (59,6 GiB) per sistema, la somma è 119 GiB, comunque oltre i 114,9 disponibili.
- Per arrivare a 64 e 64 servirebbe un disco più grande, oppure scegliere cosa tagliare (per esempio Android senza `cust`, molto rischioso e da non fare).
- Ridurre una f2fs esistente non conviene: dopo la divisione Android riformatta la sua parte (i dati Android sono già persi dal 2026-11-01). La radice iOS attuale è un'immagine di circa 20 GiB [fatto], quindi 64 GiB per iOS sono comodi, non indispensabili.

## 3. Layout proposto (scelta dell'utente: circa 55 GiB per parte)

Oggi: una sola partizione `userdata` (sda16, LUN 0) [fatto]. Calcolo sul LUN 0 da 114,90 GiB, con le altre partizioni di Android invariate (10,07 GiB, di cui `super` 8 GiB):

| Partizione | Dimensione | Contenuto |
|---|---|---|
| `userdata` | circa 49,7 GiB | dati Android (f2fs); più i 10,07 GiB di sistema Android fanno circa 60 GiB per Android |
| `iosroot` (nuova) | 55 GiB | radice iOS (HFS+) |
| `boot_android` (nuova) | 128 MiB | `boot.img` Android originale (backup in `backup/surya/boot.img`, checksum verificato [fatto]) |
| `boot` | invariata | Enxerto + payload iOS |
| tutto il resto | invariato | `recovery`, `super`, modem, persist, bootloader, backup |

I 55 GiB sono un parametro: se per "55 e 55" intendi i dati di Android, `iosroot` scende a circa 49,7 GiB. Da decidere prima di scrivere la GPT.

Regole sulla GPT (altrimenti il telefono può non avviarsi):
- la GPT ha anche la copia secondaria in fondo al LUN: vanno aggiornate tutte e due, con CRC corretti;
- backup della GPT letto dal telefono prima di scrivere (il file `backup/surya/gpt-sda.txt` è vuoto perché `gdisk` mancava sul telefono; la tabella è però completa in `lun-sda-head1M.img`);
- strumento dedicato con `--dry-run`, e un OK esplicito prima di ogni scrittura;
- `userdata` resta il nome della partizione Android (il suo `fstab` la cerca per nome [ipotesi, da verificare]).

## 4. Il selettore di avvio (boot picker) in Enxerto

Idea dell'utente: Enxerto, che già è in `boot`, guarda le partizioni del telefono, riconosce Android e iOS e avvia quello scelto, ciascuno "pulito" e con il proprio insieme di partizioni.

### 4.1 Riconoscimento (in sola lettura, usa il codice GPT e UFS che Enxerto ha già [fatto])

| Sistema | Segnali da controllare |
|---|---|
| Android | partizione `boot_android` con magic `ANDROID!` all'inizio; `userdata` con magic f2fs `0xF2F52010` a offset 1024 |
| iOS | partizione `iosroot` con volume header HFS+ (`H+` o `HX`) a offset 1024; payload iOS presente nel ramdisk di `boot` (kernel + ADT + boot-args) |

Se un sistema non supera i controlli, la sua voce non compare (o compare disattivata con il motivo). Se ne resta uno solo, parte quello senza menu.

### 4.2 Interfaccia
- Schermata con le voci trovate, più "Fastboot Bergamota"; Volume Su/Giù per spostarsi, Power per scegliere. Lettura dei tasti dal PMIC come già fa la schermata caricatore [fatto, `docs/BERGAMOTA_CHARGE.md`], da estendere al boot normale.
- Voce predefinita dopo un timeout di pochi secondi, scelta nella cmdline del boot.img (`bergamota.default=ios|android`). Niente scrittura su flash per ricordare l'ultima scelta.
- Un riavvio "verso Android" o "verso iOS" da dentro il sistema: parola magica nell'IMEM come oggi `BERG` per il fastboot [fatto]; da estendere con altre due parole.

### 4.3 Avvio di iOS
Come oggi (payload nel ramdisk di `boot`), ma montando `iosroot` per nome invece di `userdata` (sezione 5). I test restano con `fastboot boot IMG`.

### 4.4 Avvio "pulito" di Android: due strade

**Strada A, Enxerto salta direttamente al kernel Android.** Enxerto carica kernel, ramdisk e DTB da `boot_android`, usa il DTB che ABL gli ha passato (memoria e proprietà già a posto) e unisce la sua cmdline con i parametri `androidboot.*` che ABL ha aggiunto. Prima di saltare deve lasciare l'hardware come lo lascia ABL:
- spegnere USB (`usb_dwc3_stop` esiste [fatto]), MMU e cache, con pulizia delle cache;
- annullare o ripristinare quanto Enxerto cambia sulla SMMU nel passo `bergamota.smmu` [ipotesi: da verificare cosa lascia];
- il controller UFS viene reinizializzato dal driver Android [ipotesi, da verificare].
Rischi: la cmdline `androidboot.vbmeta.digest` e lo stato di verified boot calcolati da ABL riguardano `boot` (cioè Enxerto), non il `boot.img` Android; con il bootloader sbloccato di solito si prosegue, ma non provato. È la strada più flessibile e la più sperimentale.

**Strada B, far caricare Android ad ABL (più pulita).** ABL verifica e avvia già correttamente un `boot.img` Android: è la strada con cui il telefono si avviava prima. L'idea è sfruttare un percorso di ABL che carica un'altra partizione (per esempio quella di `recovery`, scelta con il motivo di riavvio `0x77665502`). Dubbi [ipotesi, da provare]: cosa aggiunge ABL alla cmdline in quel percorso, e come Android decide di non entrare in recovery. Costo: OrangeFox dovrebbe spostarsi su un'altra partizione. Un test economico prima di decidere: avviare il `boot.img` Android originale con `fastboot boot` e vedere che Android parte (era già così), poi provare il percorso con una partizione di prova, con un tuo OK.

Fino a quando non si sceglie, il ripiego semplice resta lo scambio di `boot` dall'host (`fastboot flash boot`, pochi secondi).

### 4.5 "Ciascuno vede il suo set di partizioni"
- La GPT è una sola, quindi Android e iOS **vedono** entrambi tutte le partizioni nella tabella. Un kernel Android stock non si può filtrare.
- L'isolamento è logico: Android monta solo `userdata`, `super`, `cust` e le altre sue per nome; non conosce `iosroot` e non la tocca. iOS riceve il nome di `iosroot` dal selettore e non monta altro; il fastboot di Bergamota ha una lista consentita ristretta [fatto].
- Un isolamento fisico richiederebbe una GPT diversa per sistema (scritta a ogni avvio): sconsigliato, rischioso e inutile se i due non toccano le partizioni dell'altro.

## 5. Cosa serve cambiare lato iOS

- Il kernel monta oggi la radice con `rd=ufsroot0`, cioè la partizione `userdata` intera [fatto]. Bisogna sceglierla per nome o per indice (`iosroot`). Il codice di accesso UFS e GPT esiste già in Enxerto (`partition-size:`, scansione GPT) [fatto].
- `tools/bergamota-flash.py` e Bergamota fastboot ammettono solo `userdata`, `boot` e `logo` [fatto]: `iosroot` e `boot_android` vanno aggiunte alla lista consentita, e solo in quel punto.

## 6. Rischi, e come non rischiare di brickare il telefono

**Il rischio principale è la GPT del LUN 0.** Se la tabella viene scritta male (CRC sbagliato, copia secondaria non allineata, partizioni spostate o sovrapposte), ABL o Android possono non trovare più `persist`, `super`, `modem`, `boot`. Un hard brick vero richiederebbe che anche il fastboot di Xiaomi non partisse. I bootloader (`xbl`, `abl`, `tz`) stanno su altri LUN (sdb-sdf) [fatto, dai nomi dei backup], quindi in teoria il fastboot resta raggiungibile con la GPT del LUN 0 rotta [ipotesi, non verificata: dipende da cosa ABL controlla all'avvio]. Il piano B, l'EDL di Qualcomm, sul Poco X3 NFC richiede in genere un'autorizzazione Xiaomi per scrivere [ipotesi, da verificare], quindi non va considerato una rete di sicurezza.

**Cosa lo rende sicuro (regole da rispettare):**
1. **Nessuna partizione viene spostata.** Si modifica solo la fine di `userdata` (ultima del LUN, riduzione) e si aggiungono le nuove voci `iosroot` e `boot_android` **dopo**, nello spazio liberato. `persist`, `modem`, `super`, `recovery` e le altre restano con gli stessi indirizzi, byte per byte. Nella tabella ci sono 32 posti e ne sono usati 16 [fatto].
2. Backup prima: la testa del LUN (`lun-sda-head1M.img`, già presa) **e la coda** con la GPT secondaria, più un controllo di rilettura.
3. Strumento con `--dry-run`: stampa le due tabelle (prima e dopo), verifica i CRC e le sovrapposizioni con un parser indipendente, e non scrive se qualcosa non torna.
4. Scrittura prima della copia secondaria o dell'ultima? Ordine e comportamento in caso di interruzione vanno decisi e provati; un'interruzione a metà tra le due copie è il caso peggiore.
5. Verifica dopo la scrittura: rilettura completa e confronto, poi un riavvio in fastboot di Xiaomi (Power + Volume Giù) **prima** di toccare altro.
6. Ripristino già provato: il ripristino della testa del LUN dal backup deve essere un comando che abbiamo testato su un'immagine, e va scelto un metodo che non dipenda dalla GPT che si vuole riparare (oggi Bergamota non scrive la GPT: va aggiunto, con lista consentita ristretta).
7. Un tuo OK esplicito per ogni scrittura, come già regola del progetto.

**Variante senza toccare la GPT (rischio solo di perdere dati, non di brickare):**
- Si tiene `userdata` com'è. Android usa una f2fs formattata più piccola, nella prima parte (circa 50 GiB); iOS usa la parte finale, con un offset noto, senza una partizione dedicata.
- Il kernel iOS e Enxerto leggono l'offset da una cmdline; il selettore riconosce i sistemi dai magic (f2fs in testa, HFS+ all'offset).
- Svantaggi [ipotesi]: Android può tentare di estendere o riformattare la f2fs fino a riempire la partizione (resize automatico al primo avvio, "Format Data" da OrangeFox): in quel caso cancella la parte iOS. Non c'è un confine imposto dalla tabella.
- Vantaggio: se qualcosa va storto si perdono solo dati di `userdata`, che si possono riscrivere (come già successo oggi). Serve a provare il selettore e l'avvio di Android prima di decidere se vale la pena toccare la GPT.

**Altri rischi**
- Android può provare a ridimensionare o riformattare la sua partizione al primo avvio: accettabile finché `userdata` Android è separata da quella iOS.
- Gli aggiornamenti OTA di Android non funzionano più (`boot` non è suo).
- Il dual boot è indipendente dal supporto di Wi-Fi, audio o GPU in iOS.

## 6b. Riferimento esistente: guida Windows su Poco X3 Pro (woa-vayu, n00b69)

Fonti lette il 2026-11-02: `github.com/n00b69/woa-vayu` (guide `1-partition.md`, `4-dualboot.md`, `dualboot.md`, `edl.md`; 63 stelle, nessuna licenza) e il codice Kotlin di `github.com/n00b69/woa-helper` (app per il cambio di sistema; nessuna licenza). Sono guide e codice di terzi: solo fatti e metodo, niente copia. Esiste anche una variante per il Poco X3 NFC, che qui non ho letto.

**Come fanno il dual boot Android + Windows** [fatto]:
- **Il cambio di sistema è uno scambio della partizione `boot`.** Da Android, con root, l'app fa un backup del `boot.img` di Android e poi `dd` dell'immagine UEFI su `/dev/block/bootdevice/by-name/boot` e riavvia ("QUICKBOOT TO WINDOWS"). Da Windows un programma (`sta.exe`) riscrive il `boot.img` Android salvato in `C:\` e riavvia. Non c'è un selettore all'avvio: vale l'ultima immagine scritta in `boot`. È la nostra **opzione B** (scambio di `boot`), già usata in pratica.
- **Partizionamento:** da un TWRP modificato, `sgdisk --resize-table 64` sul LUN `sda`, poi `parted`: si cancella `userdata` (nel loro caso la 32), la si ricrea più corta, e si aggiungono dopo `esp` (FAT32, 0,3 GB, flag `esp`) e `win` (NTFS, fino a fine disco). Poi si formatta `data` da TWRP e si lancia uno script `fixgpt` ("altrimenti Windows può rompere il dispositivo": Windows riscrive la GPT a modo suo). È lo stesso schema del nostro layout (`userdata` ridotta, nuove partizioni dopo, nulla spostato), quindi la procedura è nota per questa famiglia di telefoni.
- **Ogni sistema vede tutte le partizioni** nella GPT: non c'è isolamento fisico, come già avevo ipotizzato nella sezione 4.5.
- Il firmware deve essere recente per l'UEFI; non riguarda iOS.

**Cosa ci dice sul rischio** [fatto]: la guida avverte "tutti i dati verranno cancellati", "non eseguire due volte lo stesso comando", "non riavviare se pensi di aver sbagliato" e include una guida di recupero in EDL (`edl.md`). Il recupero in EDL sul Poco X3 Pro richiede lo strumento di autenticazione MiFlash HXRU a pagamento (circa 3 dollari in crediti) oppure cortocircuitare i punti di test; con il bootloader sbloccato e il fastboot raggiungibile basta `fastboot oem edl`. Conferma quanto dubitavo nella sezione 6: l'EDL non è gratuito né sicuro, quindi la GPT va toccata solo con tutte le regole di quella sezione.

**Cosa riusare per noi:**
1. La variante senza GPT (sezione 6) per iniziare, come deciso dall'utente.
2. Lo scambio di `boot` con backup automatico dell'altra immagine come primo vero dual boot, prima del selettore in Enxerto.
3. Il selettore in Enxerto resta il miglioramento: nessun `dd` da root a ogni cambio, e il cambio funziona anche se Android non parte.
4. Se un giorno si toccherà la GPT: stessa sequenza (backup, `--dry-run`, un solo comando alla volta, controllo CRC, copia secondaria).

## 7. Ordine di lavoro suggerito

1. Decidere le dimensioni definitive (sezione 3) e il sistema predefinito.
2. Salvare la GPT (sola lettura, copia primaria e secondaria) e scrivere i valori reali.
   **Prima prova consigliata: la variante senza GPT (sezione 6), che non può brickare il telefono.** Decidere se toccare la GPT solo dopo.
3. Ripiego dall'host (scambio di `boot`): costa poco, utile da subito per tornare ad Android.
4. Strumento di partizionamento con `--dry-run`, un OK esplicito prima della scrittura.
5. Nel kernel: montare `iosroot` per nome; poi riflash della radice iOS nella nuova partizione.
6. Selettore in Enxerto: riconoscimento, menu, voce predefinita, parole magiche per il riavvio.
7. Avvio di Android: provare la strada B (o la A) come lavoro separato e sperimentale, solo con l'OK dell'utente per ogni scrittura.
