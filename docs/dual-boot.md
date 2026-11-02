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

## 3. Layout proposto

Oggi: una sola partizione `userdata` (sda16, LUN 0) [fatto]. Il piano:

1. `userdata` resta il nome della partizione Android, ridotta (Android cerca quel nome nel suo fstab) [ipotesi, da verificare nel suo `fstab`].
2. Una nuova partizione subito dopo, per esempio `iosroot` (64 GiB, o quanto deciso al punto 2), per la radice HFS di iOS.
3. Una partizione piccola (circa 128 MiB) `boot_android` con il `boot.img` Android originale (il backup è in `backup/surya/boot.img`, 128 MiB, checksum verificato [fatto]).
4. `boot` continua a contenere Enxerto; `recovery`, modem, persist e tutti i backup restano intatti.

Nota: la GPT ha anche la copia secondaria in fondo al LUN. Una modifica deve aggiornare entrambe, con CRC corretti, e partire da un backup della GPT letto dal telefono.

## 4. Come si sceglie il sistema all'avvio

Enxerto (già in `boot`) sa leggere i tasti dal PMIC (PON): lo fa oggi nella schermata caricatore, dove Volume Giù avvia il fastboot e Power tenuto avvia iOS [fatto, `docs/BERGAMOTA_CHARGE.md`]. Va esteso al boot normale [ipotesi: stesso codice, `charge_mode.c`]. Una tabella di scelta possibile:

| Tasto all'avvio | Effetto |
|---|---|
| nessuno | sistema predefinito (iOS o Android, configurabile) |
| Volume Su | Android |
| Volume Giù | fastboot di Bergamota |
| Power + Volume Giù (ABL) | fastboot di Xiaomi, invariato |

### Come far partire Android da Enxerto (parte incerta)

- **Opzione A, concatenamento (consigliata come obiettivo):** Enxerto legge `boot_android`, carica kernel, ramdisk e DTB e salta al kernel, riusando il DTB che ABL gli ha passato (ha già memoria, cmdline e proprietà di ABL). Serve scrivere questo percorso in Enxerto: oggi `loader_boot_payload` avvia solo payload XNU [fatto] e non un kernel Linux [ipotesi: il formato boot.img è lo stesso già parsato per `fastboot boot`].
  - Rischi [ipotesi, non verificati]: la cmdline `androidboot.*` che ABL costruisce per il boot.img che ha verificato (qui sarebbe Enxerto) può non coincidere con quanto Android si aspetta (`androidboot.vbmeta.digest`, stato di verified boot). Con il bootloader sbloccato di solito si prosegue, ma non l'ho provato.
- **Opzione B, commutatore dall'host (ripiego semplice):** uno script `fastboot flash boot` scambia in pochi secondi l'immagine di `boot` tra Enxerto e il `boot.img` Android. Non è un vero dual boot da solo, ma non richiede nuovo codice nel bootloader. Android parte dal normale ABL.

## 5. Cosa serve cambiare lato iOS

- Il kernel monta oggi la radice con `rd=ufsroot0`, cioè la partizione `userdata` intera [fatto]. Bisogna sceglierla per nome o per indice (`iosroot`). Il codice di accesso UFS e GPT esiste già in Enxerto (`partition-size:`, scansione GPT) [fatto].
- `tools/bergamota-flash.py` e Bergamota fastboot ammettono solo `userdata`, `boot` e `logo` [fatto]: `iosroot` e `boot_android` vanno aggiunte alla lista consentita, e solo in quel punto.

## 6. Rischi

- **Una GPT sbagliata può rendere il telefono non avviabile.** Mitigazione: backup della GPT prima, controllo CRC, doppia copia, test su un backup letto dal telefono, e il fastboot di Xiaomi (Power + Volume Giù) resta sempre raggiungibile perché sta in `abl`, non toccata.
- Android può provare a ridimensionare o riformattare la propria partizione al primo avvio: va bene finché la partizione `userdata` Android è separata e non confina con quella iOS [ipotesi].
- Aggiornamenti OTA di Android sono fuori portata (`boot` non è più il suo).
- Il dual boot non riguarda il supporto di Wi-Fi, audio o GPU in iOS; è indipendente dal resto della tabella di marcia.

## 7. Ordine di lavoro suggerito

1. Decidere le dimensioni (sezione 2) e quale sistema è il predefinito.
2. Leggere e salvare la GPT (sola lettura), scrivere il documento con i valori reali.
3. Opzione B subito (costa poco, utile da subito per tornare ad Android).
4. Partizionamento: strumento dedicato con `--dry-run`, un OK esplicito prima della scrittura.
5. Montaggio di `iosroot` dal kernel, poi riflash della radice iOS.
6. Opzione A (concatenamento di Android) come lavoro separato e sperimentale.
