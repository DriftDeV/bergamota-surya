# Bluetooth sul Poco X3 NFC (surya): hardware, stato, piano

Legenda: **[V]** verificato (file/riga, patch o misura) · **[I]** inferenza · **[?]** da verificare.
Base da arricchire con piani e stato (sezioni 3 e 7). Documenti gemelli: `gpu.md`, `audio.md`, `wifi.md`, `cameras.md`.
Base di lavoro (Poco F1, stesso chip WCN3990): `docs/WIFI_BT_PLAN.md` §3, `docs/WIFI_BT_IOS18.md` §2, `docs/BT_IOS_TRANSPORT.md`.

## 1. Hardware sul Poco X3 NFC (surya)

| Voce | Dato | Fonte |
|---|---|---|
| Controller | **WCN3990**, nodo `bt_wcn3990`, compatible `qca,wcn3990` (combo con il Wi-Fi) | live.dts:866 [V] |
| Alimentazione | `qca,bt-vdd-io` (1.7-1.9 V), `-core` (1.245-1.35 V), `-pa` (3.2-3.4 V), `-xtal` (1.7-1.9 V); regolatori PMIC: io = `ldoa10` (L10A), core = `ldoc2` (L2C), pa = `ldoc10` (L10C), xtal = `ldoc1` (L1C) (risolti dai phandle) | live.dts:866-880, 16546, 16751, 16905, 16732 [V] |
| Nessun GPIO enable | il nodo BT non ha `bt-en`/GPIO: l'accensione è via «impulsi» sulla UART, come sul F1 | live.dts:866-880 [V assenza] |
| UART HCI | **`hsuart0` = `qcom,qup_uart@0x88c000`** (QUP **SE3**, `qcom,msm-geni-serial-hs`, `status = "ok"`), pin: **gpio38** (CTS), **gpio39** (RTS), **gpio40** (TX), **gpio41** (RX), funzione `qup03`, `wakeup-byte 0xfd` | live.dts:9101, 20356, 14727-14790 [V pin/nodo; I che sia il BT: l'alias `hsuart0` è la convenzione Qualcomm] |
| Console | `serial0` = SE8 (0xa88000, 2 pin): è la UART di debug, non il BT | live.dts:9293, 20355 [V] |
| Audio BT digitale | secondo SLIMbus `slim@62e40000` (`status = "ok"`), figlio `btfmslim_codec` (`qcom,btfmslim_slave`, elemental-addr `00 01 20 02 17 02`, ifd `00 00 20 02 17 02`); nel sound card `qcom,wcn-btfm = 1`, DAI `bt_sco_rx/tx` | live.dts:962-985, 17709-17717, 17953 [V] |
| Firmware (solo nomi/dimensioni) | partizione `bluetooth` (FAT, 1 MiB): `IMAGE/CRBTFW21.TLV` 230 260 B, `CRNV21.BIN` 4 710 B, `CRBTFW20.TLV` 56 180 B, `CRNV20.BIN` 3 936 B, `CRBTFW11.TLV` 29 368 B, `CRNV11.BIN` 2 798 B, `APBTFW10.TLV` 56 052 B, `APBTFW11.TLV` 230 260 B, `APNV10.BIN` 1 243 B, `APNV11.BIN` 4 875 B | `backup/surya/bluetooth.img`, listato in sola lettura [V] |
| MAC | sul F1 in `persist/wlan_bt/`; su surya da verificare; l'ADT usa un MAC sintetico | `tools/mkadt.py:38-80` [V] |

Differenze rispetto al Poco F1 [V DTB vs `WIFI_BT_PLAN.md`]: UART **SE3 0x88c000** (F1: SE6 0x898000, gpio45-48); regolatori **PM6150/PM6150L** (F1: PM8998); i file `CRBTFW21`/`CRNV21` hanno dimensioni diverse
da quelle del F1 (222 988 B / 4 591 B) [V]: versione del firmware diversa; la ROM del chip di questo telefono non è ancora letta [?].

## 2. Cosa sappiamo (verificato)

* **Sul Poco F1 il Bluetooth funziona su iOS 18**: avviso all'avvio di bluetoothd, scansione LE e **Impostazioni › Bluetooth elenca i dispositivi vicini**, controllato dall'utente
  (`docs/WIFI_BT_IOS18.md` §2.10, boot combo9) [V].
* Il WCN3990 BT è un dispositivo **HCI su UART (H4)**: nessun modem, nessun QMI. Boot: quattro regolatori; poi due «impulsi» a 2400 baud (0xC0, boot mode) e a 115200 (0xFC), attesa 100 ms; `qca_setup`: `0xFC00` param `0x19` legge product/ROM; scarica `crbtfw%02x.tlv` + `crnv%02x.bin` a segmenti; cambia baud; imposta BD address [V, `hci_qca.c` come riportato in `WIFI_BT_PLAN.md` §3].
* **Lato iOS** (A11/iOS 16 e 18): `bluetoothd` e `BlueTool` raggiungono il controller tramite un **socket di controllo del kernel** (`com.apple.uart.sk.bluetooth` / `com.apple.uart.bluetooth`),
  non con un'interfaccia IOKit; serve un nodo ADT `bluetooth` (`local-mac-address`, `transport-encoding 3`, `transport-speed 3000000`), un BlueTool «stub» (la sequenza Broadcom fallirebbe) e variabile d'ambiente `BT_HCI_TRANSPORT=H4BC:POSIX:com.apple.uart.bluetooth,3000000` (`docs/BT_IOS_TRANSPORT.md`, `WIFI_BT_IOS18.md` §2.11) [V].
* Pezzi esistenti nel repo (xnu-11215 e 12377): `0104` (probe), `0117` (keep rails), `0118` (clock UART), `0124` (firmware), `0125`/`0128` (IBS), `0132`/`0133` (post-NVM, baud), `0136` (kctl HCI), `0239` (compatibilità HCI per lo stack Broadcom-oriented di Apple), `0238` (key wrap AKS per il portachiavi), `0250` (privilegi Skywalk) [V `ls xnu/patches-11215`]. Strumenti: `tools/adt-bluetooth.py` (aggiunge `/arm-io/bluetooth`), Renegade `RENEGADE_BT_FW=DIR` (firmware in `/usr/standalone/firmware/sweet/`).
* Boot-args usati sul F1: `sweetwlan=2 sweetbt=1 sweetbtfast=1 sweetbtibs=1 sweetwlandelay=30` [V].
* **Sul surya nulla del Bluetooth è stato provato.** L'ADT surya non ha `/arm-io/bluetooth` (nodo «solo beryllium», `tools/mkadt.py:82-83`) [V]; il codice BT del F1 in `xnu/patches-11215/*beryllium-bt*` è legato ai registri dell'SE6/GCC di SDM845 [I].

## 3. Ipotesi di implementazione su Heartwood/XNU + iOS, a fasi

| Fase | Obiettivo | Come | Note |
|---|---|---|---|
| B0 | Mappa e sola lettura | GCC/clock SE3 su SM7150, TLMM gpio38-41, pin `qup03`; verifica `protocol id` UART nel `GENI_FW_REVISION_RO` (0x68); se il firmware QUP non è UART, **fermarsi** | già descritto per l'SE6 del F1 |
| B1 | Alimentare il chip | voti RPMh per L10A, L2C, L10C, L1C (`sweet_rpmh.c` oggi è cablato su SM7150: stesso RSC 0x18220000) | basso; riusa il client RPMh della GPU |
| B2 | UART grezza + impulsi | clock SE3 a 2400/115200 baud (tabelle clock SM7150), impulsi 0xC0/0xFC, lettura `0xFC00` param `0x19` | rischio medio (clock) |
| B3 | Firmware | leggere `CRBTFW21.TLV` + `CRNV21.BIN` (o 20/11 secondo la ROM) dalla partizione `bluetooth`, scarica a segmenti, cambia baud a 3 Mbaud, BD address | riusa 0124/0132/0133 |
| B4 | Lato iOS | ADT: `tools/adt-bluetooth.py`; socket di controllo (0136), compat HCI (0239), IBS (0125/0128), BlueTool stub, bluetoothd env, AKS (0238) | già fatto per il F1 |
| B5 | Connessione e audio | privilegi Skywalk (0250); audio A2DP: HAL su `com.apple.bluetooth.AudioPipe`; SCO sul SLIMbus BT (`btfmslim`) | **non provato neanche sul F1** |
| B6 | Estensioni | classic pairing, GATT, offload LE (risposte «success» finte), `/dev/btwake` | mesi |

Ordine consigliato [I]: il BT non dipende dal modem; è la prima vittoria «cheap» di connettività, subito dopo l'avvio della GPU (stesso client RPMh, stessi clock).

## 4. Ostacoli e rischi

1. **Clock QUP/UART su SM7150.** Su SDM845 le radici SE sono con DFS e richiedono la lettura della tabella; su SM7150 (GCC `gcc-sdmmagpie`) i registri e le frequenze cambiano: sbagliare significa bloccare il bus (lezione generale: leggere un blocco con clock spento blocca il telefono) [I].
2. **Il firmware QUP (`qupfw`).** La UART deve già avere il firmware di protocollo caricato dal boot; sul F1 questo è stato un punto di attenzione («SE ownership differs per engine») [V `WIFI_BT_PLAN.md`].
3. **Firmware diverso (ROM 2.1 di questo telefono)**: se la ROM è `2.0` servono `CRBTFW20/CRNV20`; la scelta dipende dalla lettura di `EDL_PATCH_VER_REQ` [?].
4. **Compatibilità HCI.** L'iOS aspetta comandi Broadcom; `0239` risponde «success» a comandi non supportati: funziona per scansione, ma gli offload LE fanno nulla [V].
5. **Audio.** Servono sia il pipe Skywalk (HAL iOS) sia il trasporto SLIMbus del WCN3990, che richiede il controller SLIMbus e l'ADSP (`docs/audio.md`) [I].
6. **Sicurezza di sistema (`AMFI`/Sandbox stand-in)**: i privilegi Skywalk 12000-12011 vanno concessi ai processi con l'entitlement giusto (0250) [V].
7. **Firmware non redistribuibile**: niente in git, solo nomi e dimensioni.
8. **Dipendenza dal Wi-Fi**: nessuna per il BT, ma i rail del WCN3990 sono condivisi (xo/rfa/ch0): spegnere l'uno non deve spegnere l'altro [I].

## 5. Lavoro della community e riferimenti utili

* **ry-diffusion/Bergamota (F1):** BT funzionante su iOS 18 (`combo9`); documenti `BT_IOS_TRANSPORT.md`, `WIFI_BT_IOS18.md`, `WIFI_BT_PLAN.md` [V, già nel nostro branch].
* **ch1tmdgus/ios-v60 (LG V60, QCA6390):** modello di diagnostica (UART, FIFO loopback, RPMh, versione QCA); chip diverso (QCA6390 usa `bt_en`/`sw_ctrl`, WCN3990 gli impulsi) [V `docs/FORKS_SURVEY.md`].
* **Linux:** `hci_qca.c` (`qca_regulator_init`, `qca_send_power_pulse`, IBS), `btqca.c`, firmware `qca/crbtfw*.tlv`, `crnv*.bin` [V].
* Nessun BT funzionante su surya nei fork censiti il 2026-11-02 [V].

## 6. Domande aperte / prossimi passi

1. Conferma che `hsuart0` (SE3, gpio38-41) sia il BT: guardare l'uso in Android (`dmesg` HCI) o il pinctrl del WCN [?].
2. Qual è la ROM del chip (`EDL_PATCH_VER_REQ`)? Quale coppia `CRBTFW/CRNV`? [?]
3. MAC del BT del telefono (persist) e come passarlo all'ADT [?].
4. Quali clock/frequenze del `gcc-sdmmagpie` per SE3 a 2400/115200/3000000 baud? [?]
5. Il protocollo del firmware QUP di SE3 è già UART dopo l'ABL? [?]

## 7. Stato

| Fase | Stato | Data |
|---|---|---|
| B0 mappa dal DTB | in questo documento | 2026-11-02 |
| B1 alimentazione | da fare | |
| B2 UART + impulsi | da fare | |
| B3 firmware | da fare | |
| B4 lato iOS | da fare (riuso dal F1) | |
| B5 audio/connessione | da fare | |
