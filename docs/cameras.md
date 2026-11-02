# Fotocamere sul Poco X3 NFC (surya): hardware, stato, piano

Legenda: **[V]** verificato (file/riga, patch, misura o ricerca documentata) · **[I]** inferenza · **[?]** da verificare.
Base da arricchire con piani e stato (sezioni 3 e 7). Documenti gemelli: `gpu.md`, `audio.md`, `wifi.md`, `bluetooth.md`.
Le fonti **[V pmOS]** vengono da una ricerca già fatta dall'utente su postmarketOS (kernel utsugi 7.1, libcamera 0.7.2), non rieseguita qui.
Il percorso reale per i frame è la pila Qualcomm CAMSS/Spectra; il lato iOS (servizio IOKit che finge l'ISP Apple) è la parte nuova.

## 1. Hardware sul Poco X3 NFC (surya)

Dal FDT vivo `dtb/surya/live.dts` (ABL Android) salvo diversa indicazione.

| Blocco | Dato | Fonte |
|---|---|---|
| Clock controller camera | `qcom,camcc-sdmmagpie` @ **0xad00000** (0x10000), vdd_cx/vdd_mx | live.dts:636 [V] |
| CSIPHY 0-3 | `qcom,csiphy-v1.2` @ 0xace0000, 0xace2000, 0xace4000, 0xace6000 (0x2000 ciascuno), regolatori `gdscr`, `refgen`, `mipi-csi-vdd` | live.dts:12359-12440 [V] |
| CCI (I²C camera) | `cam_cci0` @ **0xac4a000** (SPI 0x1cc), `cam_cci1` @ **0xac4b000**; clock `cci_*_clk`, 4 linee (DATA0/CLK0/DATA1/CLK1), modi 100k/400k/custom/1M | live.dts:12447-12540, 13031 [V] |
| Sensori | **7 nodi** `qcom,cam-sensor@0..6` (cell-index 0-6), con `csiphy-sd-index`, `cci-device`/`cci-master`, regolatori (`cam_vio`, `cam_vdig`, `cam_vana`, `cam_clk`), `eeprom-src`, `led-flash-src`, `actuator-src`, `sensor-position-roll/yaw` | live.dts:12774-13030 [V] |
| Mappa nodi | s0: CSIPHY0, CCI0 master0, flash+eeprom+attuatore · s1: CSIPHY2, CCI0 master1, eeprom · s2: CSIPHY1, CCI0 master1, eeprom (roll 270°) · s3: CSIPHY1, CCI1 master0, flash+eeprom · s4: CSIPHY2, CCI1 master0, flash+eeprom · s5: CSIPHY3, CCI0 master1, eeprom · s6: CSIPHY2, CCI0 master1, flash+attuatore, regolatori propri (vio/vana/vdig) | live.dts [V], assegnazione ai moduli fisici [I] |
| Nomi dei sensori | **non sono nel FDT** (i nodi non hanno modello). Dalla ricerca pmOS: **IMX682** principale 64 MP (CCI0 camera@1a), **HI1337** grandangolare 13 MP (CCI1 camera@21), **S5K3T2** frontale; VCM **DW9807** per l'AF; il modulo si riconosce dall'EEPROM (varianti `j20c_*` Sunny/AAC/Ofilm) | [V pmOS] |
| Attuatori/EEPROM | 3 `qcom,actuator@0..2`, 6 `qcom,eeprom@0..5` | live.dts:12543-12600 [V] |
| Flash | 3 LED PM6150L (`qcom,qpnp-flash-led-v2` + `qcom,flashlight_0`) | live.dts:5555-5660 [V] |
| IFE/CSID | `cam_csid0` 0xacb3000 / `cam_vfe0` 0xacaf000; `cam_csid1` 0xacba000 / `cam_vfe1` 0xacb6000; `csid-lite0` 0xacc8000 / `vfe-lite0` 0xacc4000 | live.dts:13323-13460 [V] |
| **ICP** (A5, Cortex-A5 32 bit) | `qcom,a5@ac00000`: `a5_qgic` 0xac00000 (0x6000), `a5_sierra` 0xac10000 (0x8000), **`a5_csr` 0xac18000** (0x3000), `fw_name = "CAMERA_ICP.elf"`; clock: `soc_fast_ahb` 200 MHz, `icp_clk_src` 400 MHz, `icp_clk` fino a 600 MHz | live.dts:13467-13485 [V] |
| **BPS / IPE** | `cam_bps` 0xac6f000, `cam_ipe0` 0xac87000, `cam_ipe1` 0xac91000 (0x3000 ciascuno); `qcom,cam-cx-ipeak` | live.dts:13487-13540 [V] |
| JPEG, FD, LRME, CDM | `qcom,cam-jpeg`, cam-cdm-intf (client vfe/jpegdma/jpegenc/fd/lrmecdm), contesti SMMU | live.dts:13124-13320, 13542 [V] |
| SMMU camera | `qcom,msm-cam-smmu`: contesti `ife`, `jpeg`, `icp`, `cpas_cdm`, `secure`, `fd`, `lrme`; **stream ID ICP** (apps SMMU): 0x1180, 0x11e0, 0x11a0, 0x1200, 0x1260, 0x1220, 0x1042, 0x1300 (mask 0x60), 0x1320 (mask 0x60); regioni iova ICP: firmware 0..0x500000, shared, … | live.dts:13124-13220 [V] |
| Memoria riservata | `camera_region@8ab00000`, **0x500000 (5 MiB), no-map**, finisce dove inizia il modem (0x8b000000) | live.dts:20471 [V] |
| GDSC | `titan_top`, `bps`, `ife_0/1`, `ipe_0/1`: tutti spenti a camera ferma | [V pmOS] |
| Firmware ICP | `/vendor/firmware/CAMERA_ICP.elf` (ELF ARM 32 bit; dimensione non letta). **Non** è nella partizione `modem`: va letto dal `/vendor` Android (come per il firmware GPU, OrangeFox in sola lettura). Solo nome | [V pmOS], `backup/surya` non lo contiene [V] |

## 2. Cosa sappiamo (verificato)

* **Sul surya nulla della camera è stato provato** con Heartwood. Nel log di iOS 18 compare `exec cameracaptured` (il demone parte, senza hardware dietro) [V `logs/surya-ios18-3.txt`]; nel port semplice la camera è fuori perimetro (`PORT_SURYA.md`, `README.md`) [V].
* **Su postmarketOS (kernel utsugi `7.1.0-rc3-sm7150`, libcamera 0.7.2 con ISP software su GPU)** le camere funzionano [V pmOS]:
  * stack: driver `qcom_camss` + `camcc-sm7150` + sensori `imx682`, `hi1337`, `s5k3t2`, VCM `dw9807_vcm`; il `camss` SM7150 e i driver HI1337/S5K3T2 vengono dalle patch di `utsugi-pmaports`; si usa il percorso **RAW (RDI)**, non il pixel path;
  * prestazioni misurate: principale **52 fps** (2312×1304, ISP software su GPU, patch libcamera 0004), grandangolare da 4-13 fps a **25-30 fps** dopo aver corretto il driver HI1337 (scriveva i registri di esposizione sbagliati), fuoco con `focus-engine`;
  * il debayer su GPU regge ~46 fps a 13 MP: il collo di bottiglia era il sensore, non l'ISP software;
  * errori «Failed to acquire camera» all'apertura: ipotesi di camere aperte due volte (barra zoom 0,5×/1×) ancora aperta.
* **L'ISP hardware (Spectra) non è usato oggi** nemmeno su Linux: il percorso RAW lascia il demosaic al software. Spectra ha due livelli [V pmOS]:
  * **Livello A (accendere):** alimentare, caricare `CAMERA_ICP.elf`, handshake HFI, ping. Il kernel Xiaomi **copia da sé i segmenti ELF**, **senza TrustZone** (`a5_core.c`: `cam_icp_validate_fw`, `cam_icp_program_fw`); handshake HFI identico a quello dell'RFC Linaro (`hfi_reg.h`: `FW_VERSION`, `HOST_ICP_INIT_REQUEST`, `ICP_HOST_INIT_RESPONSE`, `SHARED_MEM_PTR`, `QTBL_PTR`);
  * **Livello B (qualità):** i comandi dei moduli (demosaic, rumore, gamma, LSC…) li costruisce **CamX**, userspace proprietario. Nel kernel Xiaomi non c'è alcuna riga di qualità immagine (`grep demosaic|gamma|rolloff|abf` in `cam_isp`: 0 risultati): il comando «elabora un fotogramma» passa solo indirizzi di buffer preparati dallo userspace (`strip_lib_out_addr`, `iq_settings_addr`, programmi CDM). Il livello A **da solo non migliora nessuna foto**.
* Linaro (luglio 2026): **RFC molto preliminare** per BPS/IPE via ICP, solo X1E80100; libcamera **non ha** un pipeline handler per Qualcomm Spectra [V pmOS].
* Tuning Xiaomi (parametri numerici di fabbrica per IMX682/HI1337/S5K3T2: livello del nero, matrici colore per temperatura, LSC, AEC/AWB) sono nei file `com.qti.sensormodule.*.bin`, `com.qti.tuned.*_global.bin` di `/vendor/lib64/camera`; si estraggono **solo i parametri numerici**, mai ridistribuiti [V pmOS].
* **Lezione generale (da evitare di ripeterla sul surya):** leggere un blocco con il suo bus clock spento **blocca il telefono** [V pmOS e GPU `docs/GPU_A630_BRINGUP.md`]: accendere i GDSC e i clock come fa `camss` prima di leggere.

## 3. Ipotesi di implementazione su Heartwood/XNU + iOS, a fasi

| Fase | Obiettivo | Come | Rischio |
|---|---|---|---|
| C0 | Mappa hardware | completare da `live.dts`: pin di ogni sensore, GPIO reset/power, clock MCLK (`cam_clk`), regolatori (PM6150/PM6150L), eeprom/attuatori | nullo (sola lettura) |
| C1 | Accendere il blocco camera | `camcc`: GDSC `titan_top`, clock `cpas_ahb`/`soc_ahb`; poi lettura di una versione (es. `a5_csr`+0 `HW_VERSION`) con clock acceso; sonda a tappe: tappa 0 sola lettura, tappa 1 avvio | medio (bus bloccato se clock spento) |
| C2 | Un sensore | CCI I²C: sequenza di power (regolatori, MCLK, reset), lettura dell'ID chip e dell'EEPROM; scelta del modulo (`j20c_*`) | medio |
| C3 | Frame RAW | CSIPHY → CSID → VFE «RDI» (come fa pmOS): un frame RAW10 in un buffer di kernel, verificato da CPU; SMMU `ife` (stream ID camera) | medio-alto |
| C4 | Livello A | `CAMERA_ICP.elf` nella memoria 0x8ab00000 (5 MiB), mapping a IOVA 0, sblocco dell'A5, handshake HFI (modulo piccolo «sonda») | medio |
| C5 | Immagine elaborata | scelta: ISP **software** (libcamera/GPU, richiede la GPU) **oppure** livello B (CamX riprodotto, enorme) | alto |
| C6 | Lato iOS | classe IOKit che finge il servizio ISP Apple (`AppleCamIn`/famiglia ISP `H11`, il telefono è `iPhone11,8` = A12) verso `mediaserverd`/`cameracaptured`/AVFoundation, consegnando frame (BGRA/YUV) | molto alto |
| C7 | Controllo e qualità | 3A (esposizione, fuoco, bianco), tuning dai parametri numerici estratti, LSC/gamma/rumore | alto |

Ordine consigliato [I]: C0 → C1 → C2 → C3 con un solo sensore (il frontale, il più semplice, o il principale); C4/C5/C6 solo dopo GPU e audio.

## 4. Ostacoli e rischi

1. **Il lato iOS è il vero muro.** iOS non parla con il sensore: parla con l'ISP Apple (famiglia `H11` per A12). Dobbiamo scrivere un driver IOKit che finga quell'interfaccia; nessun precedente nel repo né nella community [I].
2. **Qualità senza CamX.** Per l'immagine elaborata servono demosaic, rumore, tono, LSC: o ISP software (CPU/GPU, dopo la GPU) o riprodurre i comandi CamX (registrazione e riproduzione, non sviluppato) [V pmOS].
3. **Reset/blocco del SoC** per letture a blocchi spenti (camcc, CSIPHY, ICP): regola GPU/pmOS.
4. **Sequenza di power dei sensori:** regolatori + MCLK + reset specifici per modulo; 7 nodi sensore con regolatori diversi (s6 ha regolatori propri) [V].
5. **Firmware.** `CAMERA_ICP.elf` è Qualcomm/Xiaomi: da leggere dal telefono e **tenere fuori da git**; i file di tuning solo come parametri numerici [V].
6. **SMMU e stream ID.** I 9 stream ID ICP e i contesti `ife/jpeg/icp` vanno programmati sulla stessa SMMU delle altre periferiche: errore = reset (come per la GPU) [V].
7. **Memoria.** Il pool di 5 MiB a 0x8ab00000 va dichiarato no-map nell'ADT prima del boot; dopo il livello A è usato dal firmware ICP [V DTB].
8. **Carico CPU e GPU.** Il debayer e il 3A in software competono con SpringBoard e backboardd, già saturi senza GPU [V avvio iOS 18].
9. **Concorrenza di camere:** problema «Failed to acquire camera» già visto su pmOS (due camere condividono `msm_vfe0_video0`) [V pmOS].

## 5. Lavoro della community e riferimenti utili

* **utsugi-pmaports** (pmOS surya): patch `camss` SM7150, driver HI1337 (`0099`) e S5K3T2, libcamera `0003/0004`, `plasma-camera` `0002` [V pmOS].
* **MiCode/Xiaomi_Kernel_OpenSource**, ramo `surya-q-oss`, `drivers/media/platform/msm/camera` (127 k righe; `cam_icp` 12 k, ~47 k con le dipendenze `cam_cpas`, `cam_smmu`, `cam_req_mgr`; ION in 556 righe, `cx_ipeak`, `msm_bus`) [V pmOS].
* **RFC Linaro** BPS/IPE via ICP su X1E80100: struttura del driver; manca YAML e uAPI [V pmOS].
* **libcamera:** ISP software su GPU (simple pipeline), patch di esposizione/AWB della ricerca (`0006-0008`), calibrazioni (CCM, livello del nero) per IMX682/HI1337 [V pmOS].
* **Nessun lavoro iOS-su-Qualcomm con camera** nei fork censiti il 2026-11-02 (ry-diffusion, dipa-studio, mxtpasontop e forks): la camera è fuori perimetro anche sul F1 [V].
* Lezioni dalla GPU: ogni accesso a un blocco con il clock spento è letale; il firmware ICP si carica **senza TrustZone** (diverso dallo zap GPU e dall'ADSP).

## 6. Domande aperte / prossimi passi

1. Mappa precisa nodo FDT → modulo fisico (7 nodi per 5 camere): leggere EEPROM e `j20c_*` [?].
2. Quale GPIO/regolatore fa il reset di ogni sensore? (`pinctrl` `cam_sensor_*`, FDT:5854-5870) [?]
3. Dimensione e versione di `CAMERA_ICP.elf`; è lo stesso per tutti i moduli? [?]
4. Il percorso RDI è sufficiente per iOS (frame RAW10)? Quale formato vuole `cameracaptured`? [?]
5. Quale servizio IOKit chiede `mediaserverd` per aprire una camera (analisi di `cameracaptured`/AVFCapture sul root iOS 18.7.8 già installato)? [?]
6. Esiste una via più corta del finto ISP Apple: un'app che apre direttamente il nostro user client (senza AVFoundation) per una demo? [I]

## 7. Stato

| Fase | Stato | Data |
|---|---|---|
| C0 mappa dal DTB | in questo documento | 2026-11-02 |
| C1 camcc/GDSC + lettura | da fare | |
| C2 un sensore | da fare | |
| C3 frame RAW | da fare | |
| C4 ICP (livello A) | da fare | |
| C5 immagine elaborata | da fare | |
| C6 lato iOS | da fare | |
