# Poco X3 NFC (surya): cosa resta da fare

Aggiornato il 2026-11-02. Ordine indicativo, non promesse di date. Stato attuale in `docs/SURYA_STATUS.md`.
Ogni scrittura sul telefono (`userdata`, `boot`, GPT) richiede l'OK esplicito dell'utente.

## 1. GPU Adreno 618: dal primo comando al rendering (priorità attuale)
1. **Tenere acceso GPU e zap dopo il test** (oggi il test fa `SHUTDOWN` e porta la GPU in slumber): GMU keep-alive, OOB GPU tenuto, zap in esecuzione, come fa il patch 0180 sul Poco F1. Verifica: due esecuzioni consecutive del test dello stesso CP, nessun reset, uptime lungo.
2. **Primo buffer di comandi reale**: un indirect buffer con un calcolo o un disegno semplice, e lettura del risultato dalla memoria.
3. **Client kernel con ioctl nella forma di `msm`** (modello patch 0182/`SweetAdreno630`): memoria GPU (GEM e IOVA tramite SMMU), sottomissione di buffer di comandi, fence, mappature per processo.
4. **Userland**: Mesa Turnip con la voce a618 (da verificare nel Mesa 26.2.5 usato dal progetto), poi Chlorophyll (Metal su Turnip, già in esecuzione sul Poco F1) adattato alla a618: GMEM da 512 KiB, `highest_bank_bit` 14, tabella `hwcg`.
5. **IOSurface e compositing**: far disegnare CoreAnimation/SpringBoard dalla GPU, poi fluidità (oggi tutto è su CPU a 30 Hz).
6. Più avanti: copertura Metal (riferimento: `steelbrain/metal2vulkan`, `docs/gpu.md` 5.4), shader, prestazioni.
- Rischi noti: reset senza panic se si lascia la GPU in uno stato incoerente; zap e SECVID; differenze a618/a630; iOS 18 e iOS 26 hanno offset IOGPU diversi.

## 2. Versioni di iOS
- **iOS 26.x** (xnu-12377, `xnu/patches-12377`): la radice è già scaricata in `research/ios26/`; da portare patch e ADT. Poi **iOS 27** per gradini. Ordine scelto dall'utente: 18, poi 26, poi 27.
- **Display**: eliminare la barra di stato rossa senza passare al 3x (più lento e con layout rotto).
- **Riavvio da iOS** (`sweetreboot fastboot`/`bergamota`): le patch 0137/0140 usano ancora l'IMEM del Poco F1 (`0x146bf65c`) e il PMIC PM8998; per surya l'IMEM è `0x146aa65c`. Da portare con un boot-arg, così `sweetreboot` funziona anche dal sistema acceso.

## 3. Audio, Wi-Fi, Bluetooth, fotocamere (dettagli in `docs/audio.md`, `wifi.md`, `bluetooth.md`, `cameras.md`)
- **Audio**: codec WCD9375 su SoundWire con macro bolero e amplificatore TAS256x; servono ADSP (PAS), SoundWire e un driver audio iOS. Stima: settimane o mesi.
- **Wi-Fi**: WCN3990 via `icnss@18800000`, modem PAS id 4, firmware dalla partizione `modem`.
- **Bluetooth**: probabilmente `hsuart0` (SE3), firmware CRBTFW21/CRNV21.
- **Fotocamere**: IMX682, HI1337, S5K3T2; priorità più bassa. La ricerca personale dell'utente è in `POCO_X3_CAMERE_ISP.md` (non committata, da sanificare prima di condividerla).
- Riferimento utile per tutti: il devicetree di `sm7150-mainline/linux` (v7.2) e il wiki postmarketOS (touch, Wi-Fi, Bluetooth e 3D "Works" su mainline).

## 4. Dual boot Android + iOS (idea futura, `docs/dual-boot.md`)
- Decisione dell'utente: **per ora solo `userdata`, nessuna modifica alla GPT**. Android in una f2fs più piccola in testa e iOS in coda con offset, oppure scambio di `boot` dall'host con backup automatico dell'altra immagine.
- In seguito, selettore di avvio in Enxerto (riconoscimento di Android e iOS dalle partizioni, avvio pulito di Android: due strade da provare). Dimensioni scelte: circa 55 GiB per parte.

## 5. Compilazione e infrastruttura
- **Macchina di compilazione propria** per dipendere meno dalla CI: prova con VMware Fusion 13.6.4 e guest Sequoia 15.6+ (serve login Broadcom e Apple ID per Xcode, a carico dell'utente); in alternativa un Mac mini Apple Silicon usato o un noleggio orario per misurare i tempi.
- Ibrido su macOS 13: Command Line Tools di Xcode 16.2 più `iig` e KDK da Xcode.app (da verificare).
- Runner self-hosted GitHub (il workflow parte solo con `workflow_dispatch`, ha già il parametro `runner`).

## 6. Decisioni aperte
- Licenza del codice GPU derivato da `mxtpasontop/ios-sweet` (nessun file di licenza, attribuzione mantenuta): tenere o rimuovere.
- Cosa fare di `POCO_X3_CAMERE_ISP.md` (sanificare o lasciare fuori).
- Riabilitare la CI per iOS 16 con la patch opzionale xcode16 (`xnu/optional/0086-xcode16-iig-iorpc.patch`).
- Dimensioni definitive del dual boot e sistema predefinito.
