# Poco X3 NFC (surya) su postmarketOS — fotocamere e ISP: tutto quello che abbiamo imparato

Fonte principale: la chat «PostmarketOS per Poco X3 NFC» (ultimo aggiornamento 12 ott 2026) + memoria salvata.
Legenda: **[V]** verificato con dati/codice nella chat · **[I]** inferenza o ipotesi · **[?]** non so come è finita.

---

## 0. In 30 secondi

- **Obiettivo**: framerate decente su principale e grandangolare + buona gestione delle alte luci, a gradini.
- **Risolto**: grandangolare da 4–13 fps a **25–30 fps** (driver HI1337 scriveva i registri sbagliati); principale a **52 fps** (un nostro test aveva lasciato il blanking al massimo); fuoco (mancava `v4l2-ctl`).
- **Pronto ma non installato (a quanto so)**: patch libcamera `0006` (AWB), `0007` (esposizione veloce), `0008` (bersaglio più basso, opzionale).
- **ISP hardware (Spectra: ICP + BPS + IPE)**: capito come funziona, **messo in pausa**. Avviarlo è fattibile, **farlo lavorare bene no**: i comandi di qualità li costruisce CamX, proprietario.
- **Strada scelta**: ISP software potenziato (libcamera su GPU) con i parametri di tuning estratti dai file Xiaomi.

---

## 1. Hardware e software di partenza

| Voce | Valore |
|---|---|
| Telefono | Xiaomi Poco X3 NFC, `surya`, Snapdragon 732G (SM7150) |
| OS | postmarketOS edge, Plasma Mobile, kernel utsugi **7.1.0-rc3-sm7150** (r267) |
| Fotocamere viste da Linux | **IMX682** (principale, 64 MP, bus CCI `cci@ac4a000`, camera@1a) · **HI1337** (grandangolare, `cci@ac4b000`, camera@21) · **S5K3T2** (frontale) |
| Autofocus | VCM **DW9807** (`12-000c`) |
| Modulo ottico | Android: `j20c_*` (varianti Sunny / AAC / Ofilm; il modulo si riconosce dall'EEPROM) |
| Driver kernel camera | `qcom_camss` + `camcc-sm7150`; driver sensori `imx682`, `hi1337`, `s5k3t2`, `dw9807_vcm` |
| Stack | libcamera 0.7.2 con **ISP software su GPU** + app `plasma-camera-utsugi` |
| Note | Il `camss` SM7150 e i driver HI1337/S5K3T2 vengono dalle patch di **utsugi** (`utsugi-pmaports`), non dal mainline puro |

Grafo media: `msm_csiphy0-3`, `msm_csid0-1`, `msm_vfe0/1/2_rdi0-2` + `msm_vfe2_pix`, sensori. Oggi si usa il percorso **raw (RDI)**, non quello pixel.

---

## 2. Perché le foto erano brutte (sintomi → cause)

| Sintomo | Causa trovata | Prova | Stato |
|---|---|---|---|
| Messa a fuoco ferma | Mancava `v4l2-ctl` per il servizio `focus-engine` (utsugi) | log `did-not-take … v4l2-ctl: not found` | ✅ risolto [V] |
| «Failed to acquire camera» all'apertura | Ipotesi: WirePlumber/PipeWire tengono aperte le camere; oppure l'app apre **due** camere (barra zoom 0,5×/1×) che condividono `msm_vfe0_video0` | `fuser`; il fix WirePlumber (`50-niente-fotocamere.conf`) non ha risolto | ⏳ **aperto**, causa più probabile: codice della barra zoom che ripristina la camera sbagliata all'avvio [I] |
| Principale a 2–4 fps | Un nostro test di esposizione massima aveva lasciato il **blanking verticale** a 58591 (quadro da 59671 righe): 190 × 1456 / 59671 ≈ 4,6 fps | calcolo = misura; dopo il ripristino **189 fps raw** | ✅ risolto, mirino **52 fps** [V] |
| **Grandangolare a 4–13 fps** | Il driver scriveva i registri sbagliati: la "esposizione" regolava la **lunghezza della riga** invece del tempo di integrazione | formula `72 MHz ÷ (riga × quadro)` su 8 punti, errore <0,6 %; lettura registri; test a runtime | ✅ driver corretto: **25–30 fps** [V] |
| Grandangolare non scende sotto ~1 ms | Con quei registri l'integrazione non poteva essere breve | luminosità lineare in 5 punti | ✅ risolto |
| Sotto 4 righe il sensore non integra meno | Minimo reale del sensore | fit con errore 0,06 | 🟡 patch pronta (vedi §6) |
| **Verde all'aperto** (principale) | L'AWB fa la media di **tutti** i pixel, compreso il cielo bruciato (22,7 % saturi) | codice `awb.cpp` + calcolo sulla tua foto | ⏳ patch `0006` pronta, non installata |
| **Esposizione lenta** | Correzione max 10 % ogni 4 fotogrammi (guadagno 0,04): 7–13 s per adattarsi | simulazione dell'algoritmo reale di `agc.cpp` | ⏳ patch `0007` pronta |
| Immagine troppo chiara | Bersaglio al 38 % lineare (≈166/255); 9,6 % di pixel vicini alla saturazione | calcolo sull'istogramma | ⏳ patch `0008` opzionale |
| Colori sballati (soprattutto grandangolare, viola/rumore) | Tre candidati: AWB falsato dai pixel bruciati (0006), calibrazione colore installata, sensore | serve confronto raw vs app (`wide-raw-capture.sh`) | ⏳ **aperto** |

### Numeri di riferimento

- **Grandangolare**: frame length predefinito `vts = 3307` → ~19,5 fps (misura dell'autore del driver). Variante "veloce" del firmware Xiaomi: `vts_min = 1662` → ~39 fps (**stima** 19,5 × 3307 / 1662 ≈ 38,8) [I].
- **Principale**: utsugi misura 52 fps a 2312×1304 con l'ISP su GPU (patch libcamera `0004`).
- Il **debayer su GPU** regge ~46 fps a 13 MP: il limite era il sensore, non l'ISP software.
- Esposizione automatica nella nuova patch: guadagno proporzionale da 0,04 a **0,20**, passo massimo da 0,15 a **0,50**. Adattamento: interno→finestra **6,8 s → 1,1 s**; finestra→interno 11,1 s → 2,1 s (simulato, ritardi sensore testati fino a 9 fotogrammi; quello vero è 2).
- Bersaglio `0008`: da 2,5 a **2,0** → ~30 % di luce in meno, pixel vicini alla saturazione dal 9,6 % al 4,3 % (0 % su finestra luminosa).

### Piste sbagliate (non rifarle)

| Pista | Perché era sbagliata |
|---|---|
| «Statistiche rotte» sulla principale (AnalogueGain 22,26 fisso, ColourGains 1/1, 8890 K fissi) | Il telefono era **a faccia in giù**. Chiedere sempre come è appoggiato |
| Bug GPU, livello del nero, matrici che rompono le statistiche | Esclusi dai test (livello del nero = 64, giusto) |
| ISP «collo di bottiglia» a 13 MP | Il limite era il sensore |
| Dimensioni di uscita `(L−4)` | Per RAW10 il pattern è largo 4: corretto è `(L−8)` |
| «Stanza troppo scura» | Aveva ragione l'utente: a 1 ms servono pochi conteggi |

---

## 3. Il driver HI1337 (grandangolare)

- Origine: patch utsugi `0099-the-wide-angle-hi1337-driver.patch`.
- Fix nostro: **`0201-hi1337-exposure-is-the-coarse-integration-time.patch`**.
  - Esposizione → registro **`0x020a`**, in **righe** (coarse integration time).
  - Default **1000** righe; minimo: 720 (originale) → 100 (prima versione, 1 ms a 720 per riga) → **4 (finale)**.
  - `HBLANK` resta **provvisorio**: la riga reale è 720 / 72 MHz, che libcamera non vede, quindi l'`ExposureTime` riportato è sbagliato del rapporto tra i due. Unità ancora da fissare.
- Script `build-hi1337.sh`: compila il kernel utsugi con la patch, estrae `hi1337.ko*`, lo installa sul telefono con backup, `depmod`.
  - Backup sul telefono: `hi1337.ko.zst.originale`, `hi1337.ko.zst.v1-min100`.
  - Controllo: `v4l2-ctl -d <subdev hi1337> -l | grep exposure` → deve dire `min=4`.
  - Errore visto: `pmbootstrap` fallisce su `sudo losetup --json --list` (non risolto nella chat) [?].
  - Rollback: copiare `.originale` sul modulo attivo, `depmod`, riavvio.
- **Da fare**: guadagno analogico di HI1337 → servono i file `.so` Xiaomi `com.qti.sensor.sunny_hi1337.so` e `…aac_hi1337.so` (non ancora analizzati) [?].

---

## 4. Messa a fuoco

- Servizio **`focus-engine`** (pacchetto `utsugi-surya-camera`): tiene **sempre aperto** il DW9807; se nessuno lo tiene aperto, il driver riporta la lente all'origine e spegne il motore.
- Config nostra: `/etc/systemd/system/focus-engine.service.d/smooth.conf` (fuoco "dolce").
- App: **tocca per mettere a fuoco** (mirino bianco = cerca, verde = ok, rosso = fallito); prova più posizioni e sceglie la più nitida (`plasma-camera-utsugi`, patch `0002`). **Non c'è autofocus continuo.**
- Richiede `v4l2-ctl` e `i2ctransfer` (installati).

---

## 5. ISP hardware Qualcomm Spectra — cosa sappiamo

### 5.1 Architettura sul tuo chip

| Blocco | Ruolo | Nel kernel pmOS oggi |
|---|---|---|
| **IFE** (`vfe175_130`, lite `vfe-lite175`, CSID `csid175_200`) | Percorso in linea dal sensore. Pixel path = demosaic/colore/statistiche in hardware | ✅ `camss-vfe-17x` lo gestisce, ma usiamo solo **raw (RDI)**; `msm_vfe2_pix` esiste nel grafo ma non è usato |
| **ICP** (`qcom,cam-a5`, **Cortex-A5 a 32 bit**) | Processore che esegue il firmware `CAMERA_ICP.elf` | ❌ nessun driver |
| **BPS** (Bayer) e **IPE** (qualità) | Elaborano fotogrammi dalla memoria, comandati dall'ICP | ❌ nessun driver; nel DT sono solo **finestre di registri dentro `camss`** |
| Moduli di qualità | Demosaic, rumore, gamma, LSC… | Programmati da **CamX** (userspace proprietario) |

### 5.2 Fatti verificati [V]

- Il firmware **c'è** sul telefono: `/vendor/firmware/CAMERA_ICP.elf` (ELF ARM 32 bit).
- **Sorgente kernel Xiaomi pubblico (GPL)**: `MiCode/Xiaomi_Kernel_OpenSource`, ramo `surya-q-oss`, `drivers/media/platform/msm/camera` (127 mila righe; `cam_icp` 12 mila, ~47 mila con le dipendenze).
- Il **kernel carica il firmware da solo** copiando i segmenti ELF (`a5_core.c`: `cam_icp_validate_fw`, `cam_icp_program_fw`): **niente TrustZone** da superare, diversamente dall'RFC Linaro.
- **Handshake HFI** identico nella struttura a quello dell'RFC Linaro (`hfi_reg.h`: `FW_VERSION`, `HOST_ICP_INIT_REQUEST`, `ICP_HOST_INIT_RESPONSE`, `SHARED_MEM_PTR`, `QTBL_PTR`).
- **Zero righe di qualità immagine nel kernel** (`grep demosaic|gamma|rolloff|abf` in `cam_isp`: 0 risultati). Il comando «elabora un fotogramma» passa al firmware solo **indirizzi di buffer preparati dallo userspace**: `strip_lib_out_addr`, `iq_settings_addr`, programmi CDM (`cdm_prog_base`, `cdm_pre_ltm`, `cdm_post_ltm`, `cdm_anr_*`, `cdm_tf_*`).
- Linaro, luglio 2026: **RFC molto preliminare** per BPS/IPE via ICP, **solo X1E80100** (laptop Snapdragon X). L'autore scrive «YAML is missing, uAPI documentation is missing».
- libcamera **non ha** un pipeline handler per Qualcomm Spectra.

### 5.3 Due livelli

| Livello | Cosa | Difficoltà | Informazioni |
|---|---|---|---|
| **A. Accendere** | Alimentare, caricare il firmware, handshake HFI, ping | 🟡 medio | Complete e pubbliche (sorgente Xiaomi + RFC Linaro) |
| **B. Farli lavorare bene** | Costruire i comandi dei moduli di qualità | 🔴 molto alto | Dentro **CamX**, binario chiuso. Il vero reverse engineering |

Il livello A **da solo non migliora nessuna foto**.

### 5.4 Strade valutate

| Strada | Costo | Esito |
|---|---|---|
| A. Portare tutto lo stack camera Android | 127 k righe, sostituisce `camss` | Scartata |
| B. Portare solo ICP/BPS/IPE | ~47 k righe; `cam_icp` dipende da `cam_cpas` (685 punti d'uso), `cam_smmu`, `cam_req_mgr`; **ION in 556 righe** (non esiste più nel kernel moderno), `cx_ipeak` (67), `msm_bus` (12) | Scartata per ora; senza CamX nessuno compone i comandi |
| **C. Modulo piccolo fuori albero** (sonda) | Qualche migliaio di righe | ✅ Scelta per rispondere a «l'A5 si avvia sul 7.1?» |
| Halium/libhybris con CamX | CamX parla col kernel Android 4.14 (`cam_req_mgr`) | Non verificata; richiederebbe un kernel Android e butterebbe via il lavoro di utsugi [I] |

Idea per il livello B: **registrare e riprodurre** i comandi CamX (non sviluppata oltre).

### 5.5 Dati hardware per la sonda (verificati sul telefono)

| Voce | Valore |
|---|---|
| Nodo camss | `soc@0/camss@ace0000`, compatible `qcom,sm7150-camss` |
| `reg-names` del camss | csiphy0-3, vfe0, vfe1, vfe_lite, **bps**, camnoc, cpas_cdm, cpas_top, **ipc_csr**, ipc_qgic, ipc_sierra, **ipe0**, **ipe1**, jpeg_dma, jpeg_enc, lrme |
| Registri ICP/A5 | `a5_csr` a **0xac18000** (HW_VERSION a offset 0) |
| Clock controller | `clock-controller@ad00000` (phandle `0x9b`, 64 KiB), driver `camcc-sm7150` |
| Costanti camcc (mainline `qcom,sm7150-camcc.h`) | `CAMCC_ICP_CLK`=45, `_CLK_SRC`=46, `_AHB_CLK`=44, `CAMCC_FAST_AHB_CLK_SRC`=40; GDSC: `TITAN_TOP`=5, `BPS`=0, `IPE_0`=3 |
| Frequenze (sorgente Xiaomi) | ICP **400 MHz (SVS)**, fast_ahb **200 MHz** |
| GDSC oggi | `camcc_titan_top`, `bps`, `ife_0/1`, `ipe_0/1`: tutti **off** a camera ferma |
| Memoria riservata | `camera@8ab00000`, **5 MiB (0x500000)**, `no-map`; identica a Xiaomi; finisce dove inizia il modem. **Va dichiarata nel DT prima del boot** |
| SMMU | `15000000.iommu` (apps_smmu); gruppo IOMMU 10 condiviso da tutti i flussi del camss |
| Stream ID IFE | 0x800, 0x880, 0x900, 0x8a0, 0x8c0, 0x940 |
| Stream ID ICP (9) | 0x1180, 0x11A0, 0x11E0, 0x1200, 0x1220, 0x1260, 0x1042, 0x1300, 0x1320 (ultimi due maschera 0x60). **Ci sono già tutti nel camss** (30 record; JPEG 0x1280–0x12e0 maschera 0x20) |
| Sorgente DT Android | `sdmmagpie-camera.dtsi` (ICP righe 389-400) |
| Strumenti sul telefono | `dtc`, `fdtget`, `fdtoverlay` presenti; **non si compila il kernel sul telefono** (niente `build/` né `gcc`) |

### 5.6 Piano della sonda (progettato, **non so se eseguito** [?])

Principio di sicurezza: nodo DT nuovo **senza `reg`** (la finestra `ipc_csr` è già nel camss; due nodi sulla stessa finestra possono confliggere). Si esegue **con l'app camera chiusa**, via SSH.

| Tappa | Cosa fa | Se riesce | Se fallisce |
|---|---|---|---|
| **0 – lettura sicura** | Accende `titan_top_gdsc` + clock ICP (come li accende `camss`, per evitare di leggere con bus clock spento), legge `HW_VERSION` da `a5_csr`, spegne | Il blocco è raggiungibile da Linux | Il telefono si blocca: tieni premuto Power ~15 s. Accesso protetto o sequenza clock errata |
| **1 – avvio** | Copia l'ELF nei 5 MiB, mappa a IOVA 0, sblocca l'A5, handshake HFI + ping | L'A5 gira sul kernel 7.1 | Il log dice a quale passo si ferma |

Strumenti: modulo `.ko` fuori albero (stesso metodo di `build-qg.sh` / `qcom_qg`), nodo DT con `fdtoverlay`, protezione dagli aggiornamenti con `surya-fixes`, rollback ricopiando il `.dtb` originale.
Dettaglio da controllare alla tappa 1: l'allocatore IOVA non deve dare a qualcun altro la zona 0–5 MiB.

### 5.7 Verdetto per i tuoi obiettivi

| Obiettivo | Serve ISP hardware? |
|---|---|
| Alte luci | ❌ No: è un algoritmo |
| Framerate | ❌ Probabilmente no; i colli di bottiglia trovati erano di software/driver |
| Qualità «da Android» (rumore, tono locale, LSC…) | ✅ Sì, oppure riscrittura in software |

---

## 6. Tuning Xiaomi e ISP software potenziato

### 6.1 File sul telefono Android (`/vendor/lib64/camera`)

- `com.qti.sensormodule.j20c_*{imx682,hi1337,s5k3t2}*.bin` → ritardi di esposizione/guadagno, livello del nero, formula del guadagno, tempi di riga.
- `com.qti.tuned.j20c_*{imx682,hi1337,s5k3t2}*_global.bin` → tuning completo (es. `com.qti.tuned.j20c_ofilm_imx682_wide_global.bin`).
- `com.qti.sensor.*_hi1337.so` → per il guadagno analogico (da analizzare).
- Raccolti in `~/surya-camera/` sul PC, backup in `~/surya-backup/` (con `dtb-android`, `super-parts/vendor.img`).
- Regola: si estraggono **solo i parametri numerici**; i file Xiaomi non si ridistribuiscono.

### 6.2 Moduli di tuning estratti (IMX682)

| Fase | Moduli | Cosa fanno |
|---|---|---|
| Bayer | `pedestal13` `bls12` `linearization33/34` | Livello del nero |
| | `bpcbcc50` `pdpc11/20` `gic30` | Pixel difettosi, PDAF, squilibrio verdi |
| | `abf34/40` | Rumore Bayer |
| | **`lsc34`** | Ombreggiatura lenti, per temperatura colore |
| | `hdr20/22/23` `gtm10` | HDR, tone mapping globale |
| | `demosaic36/37` | Demosaicizzazione |
| Colore | **`cc12/cc13`** | Matrici colore per temperatura ✅ **già estratte** |
| | **`gamma15/16`** | Curve gamma |
| | `cst12` `cv12` `tdl10` `sce11` `cs20` | Conversione colore, LUT tinta/saturazione, pelle, crominanza |
| IPE | `anr10` `tf10` `hnr10` | Rumore spaziale/temporale/ibrido |
| | `ltm13` `tmc10/11` | Tone mapping locale |
| | `asf30` | Nitidezza |
| | `cac22` `ica10/20` `lrme10` `gra10` `upscale20` `tintless20` | Aberrazione, geometria, movimento, grana, ingrandimento |
| 3A | `luxLumaTargetZone`, `CCTBasedExpComp`, `histLumaTargetZone` | Obiettivo di luminosità, protezione alte luci |
| | `Grey stats`, `CCTTables`, `SA Outdoor Low-CCT` | Bilanciamento del bianco |

Quasi ogni modulo ha varianti `_aec_data` e `_cct_data`: i parametri **interpolano** in base a luce e temperatura colore.
Ogni modulo ottico ha la sua taratura (Sunny / AAC): per sapere quale hai si legge l'EEPROM. Nelle matrici di principale e grandangolare le due varianti sono state estratte entrambe.

### 6.3 Cosa manca a libcamera 0.7.2 (ISP software)

| Modulo | Stato | Priorità |
|---|---|---|
| Livello del nero | ✅ | — |
| Matrice colore per temperatura | ✅ (estratte) | — |
| Alte luci (taglia i canali separatamente → bianchi colorati) | ❌ | 🔴 |
| Esposizione automatica (generica, lenta) | 🟡 | 🔴 |
| Bilanciamento bianco («grey world») | 🟡 | 🟠 |
| Gamma / tono (fissa) | 🟡 | 🟠 |
| LSC | ❌ | 🟠 |
| Riduzione rumore (guadagno fino a **22×**) | ❌ | 🟠 |
| Nitidezza, tono locale, pelle | ❌ | 🟡 |
| HDR, rumore temporale, multi-frame | ❌ | ⚪ fuori portata |

Upstream: la todo dell'ISP su GPU di libcamera cita rumore, pixel difettosi e LSC; FOSDEM 2026 ha parlato di calibrazione sensori, CCM e LSC → conviene allinearsi, non reinventare.

### 6.4 Piano a fasi (dalla chat)

0. ✅ Matrici di fabbrica, livello del nero, fuoco.
1. Decodificatore del tuning Xiaomi → export in YAML.
2. Esposizione/AWB (patch 0006–0008).
3. LSC, gamma, rumore.
Piano B: `sviluppa-raw.py` (sviluppo RAW con colori corretti, indipendente da libcamera) → «scatto RAW» se libcamera non basta.

---

## 7. File e patch prodotti

| File | A cosa serve | Stato |
|---|---|---|
| `0201-hi1337-exposure-is-the-coarse-integration-time.patch` | Driver HI1337 | ✅ installato (min=4 attivo) |
| `build-hi1337.sh` | Build + install modulo, con backup | ✅ usato |
| `0006-awb-leaves-clipped-pixels-out.patch` | AWB ignora pixel saturi (libcamera) | ⏳ pronta |
| `0007-agc-converges-in-a-few-updates.patch` | Esposizione veloce | ⏳ pronta |
| `0008-agc-aims-lower.patch` | Bersaglio 2,0 | ⏳ opzionale |
| `build-libcamera.sh` | Compila libcamera con una o più patch (`PATCHES=…`), default 0006+0007 | ⏳ pronto |
| `/usr/share/libcamera/ipa/simple/imx682.yaml`, `hi1337.yaml` | Tuning (CCM, livello del nero 64 → 4096 su 10 bit, «presunto, da misurare» per HI1337) | ✅ installati |
| `focus-engine` + `smooth.conf` | Fuoco | ✅ |
| `~/.config/wireplumber/wireplumber.conf.d/50-niente-fotocamere.conf` | Impedisce a WirePlumber di occupare le camere | ⚠️ presente, **efficacia non provata** |
| `wide-raw-capture.sh`, `sviluppa-raw.py` | Confronto raw/app, sviluppo RAW | disponibili |
| `surya-fixes` | Rende persistenti le modifiche al `.dtb` e ai moduli | ✅ attivo |

Telefono: `drift@192.168.1.45` (SSH). PC: Fedora, `~/Scaricati/`, `~/src/utsugi-pmaports`, `pmbootstrap`.

---

## 8. Problemi aperti (numerati come nella chat)

1. «Failed to acquire camera» (due avvisi) — ipotesi barra zoom.
2. Esposizione lenta/erratica sulla grandangolare — patch 0007 non installata.
3. Verde all'aperto sulla principale — patch 0006 non installata.
4. Colori sbagliati sulla grandangolare — serve confronto raw.
5. Rumore sul sensore grandangolare (guadagno analogico ignoto).
6. Ripristino Android — backup verificati (dtbo, boot, vbmeta, vbmeta_system, super; `fastboot` ok), sequenza di flash data; **non so se eseguita**. A novembre il telefono risulta ancora su pmOS.
7. Pull request a utsugi per la patch del gauge batteria (`qcom_qg`) — in sospeso.

### Prossimi passi consigliati (dalla chat)

1. Comando di stato («Mossa 0») per sapere cosa c'è davvero sul telefono.
2. **Una** compilazione libcamera con 0006+0007.
3. Test sul balcone: verde sparito? convergenza <3 s? fluidità invariata?
4. Solo se i colori della grandangolare restano sbagliati: confronto con il raw.
5. Rimandati: guadagno analogico, `HBLANK`, ISP hardware, patch 0008.

---

## 9. Possibili punti di contatto con il port di Bergamota (mie osservazioni [I])

- Stesso SoC SM7150: i valori `camcc`, lo SMMU applicativo `15000000.iommu` e il modo di accendere i GDSC con i clock giusti sono la stessa famiglia di problemi del bring-up GPU (Adreno 618, GMU).
- La lezione «leggere un blocco con bus clock spento blocca il telefono» vale identica per GPU e camera.
- Per il port iOS serve l'**FDT dell'ABL Android**, non quello mainline di pmOS: le due descrizioni dell'hardware non coincidono.
- Le camere non sono nel perimetro del port semplice di Bergamota (non funzionano nemmeno sul Poco F1).

---

## 10. Comandi utili

```bash
# stato camere sul telefono
media-ctl -d /dev/media0 -p | grep -E '^- entity'
cam -l
SW=$(for d in /dev/v4l-subdev*; do grep -q hi1337 /sys/class/video4linux/${d##*/}/name && echo $d; done)
v4l2-ctl -d $SW -l | grep -E 'exposure'          # min=4 = driver finale

# framerate (telefono in piedi, scena illuminata)
MAIN='/base/soc@0/cci@ac4a000/i2c-bus@0/camera@1a'
WIDE='/base/soc@0/cci@ac4b000/i2c-bus@0/camera@21'
fps() { grep -o '([0-9.]* fps)' | tr -d '(fps)' | tail -30 | awk '{s+=$1} END {printf "%.1f fps\n", s/NR}'; }
cam -c "$WIDE" --capture=60 -s role=viewfinder 2>&1 | fps

# quali clock lavora l'ISP mentre scatta
sudo grep -E 'camcc_(ife_0|ife_1|ife_lite|bps|ipe_0|icp)_clk ' /sys/kernel/debug/clk/clk_summary

# rollback driver HI1337
DST=$(find /lib/modules/$(uname -r) -name 'hi1337.ko*' ! -name '*.originale' | head -1)
sudo cp "$DST.originale" "$DST" && sudo depmod && sudo reboot
```

---

## Fonti

- Chat «PostmarketOS per Poco X3 NFC»: https://claude.ai/chat/0f1cbf52-715d-4430-ba81-bf280f0a2977
- Chat «AluminiumOS installation» (contesto ROM e backup): https://claude.ai/chat/57cb5fd0-24d7-43e0-9beb-13b6ee47a893
- Sorgente kernel Xiaomi, ramo `surya-q-oss`: https://github.com/MiCode/Xiaomi_Kernel_OpenSource (`drivers/media/platform/msm/camera/cam_icp`, `cam_isp`, `cam_req_mgr`; `fw_inc/hfi_reg.h`, `fw_inc/hfi_session_defs.h`, `icp_hw/a5_hw/a5_core.c`, `icp_hw/icp_hw_mgr/cam_icp_hw_mgr.c`)
- RFC Linaro su BPS/IPE via ICP (X1E80100, luglio 2026): https://ratatoskr.run/lkml/2026/07/17211940/t
- utsugi-pmaports (kernel, patch HI1337 `0099`, libcamera `0003`/`0004`, `plasma-camera` `0002`): https://github.com/utsugi-pmos/utsugi-pmaports
- Supporto SM7150 / Poco X3 NFC in postmarketOS: https://gitlab.com/postmarketOS/pmaports/-/merge_requests/3706
