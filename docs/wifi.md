# Wi-Fi sul Poco X3 NFC (surya): hardware, stato, piano

Legenda: **[V]** verificato (file/riga, patch o misura) · **[I]** inferenza · **[?]** da verificare.
Base da arricchire con piani e stato (sezioni 3 e 7). Documenti gemelli: `gpu.md`, `audio.md`, `bluetooth.md`, `cameras.md`.
Base di lavoro: Wi-Fi iOS 26 del Poco F1 (WCN3990): `docs/WIFI_IOS26.md`, `docs/WIFI_IOS26_PHASE2.md`, `docs/WIFI_BT_PLAN.md`,
`docs/WIFI_BT_IOS18.md`, `docs/WIFI_KEXT_STUDY.md`.

## 1. Hardware sul Poco X3 NFC (surya)

| Voce | Dato | Fonte |
|---|---|---|
| Chip | **WCN3990** (stesso del Poco F1), collegato via **SNOC**, nodo `qcom,icnss@18800000` (non PCIe) | live.dts:5972 [V] |
| icnss | `membase` 0x18800000 (0x800000), `smmu_iova_base` 0xa0000000 (0x10000000), `smmu_iova_ipa` 0xb0000000 (0x10000), `iommus = <apps_smmu 0x240 0x01>`, `qcom,smmu-s1-bypass`, 12 IRQ (SPI 0x19e-0x1a9) | live.dts:5972-5990 [V] |
| MSA | `qcom,wlan-msa-memory = 0x100000`, regione fissa `wlan_msa_region@97500000` (0x180000, no-map) | live.dts:5979-5980, 20506 [V] |
| Alimentazione | `vdd-cx-mx` (0x9c400 µV), `vdd-1.8-xo`, `vdd-1.3-rfa`, `vdd-3.3-ch0`, `vdd-3.3-ch1` (stesse fonti del BT: xo/rfa/ch0 = quelle del nodo `bluetooth`) | live.dts:5984-5988, 866-880 [V] |
| SMP2P | `smp2p_wlan_1_in` (`qcom,entry-name = "wlan"`), eventi `force-fatal-error`, `early-crash-ind` | live.dts:4339, 5989-5992 [V] |
| Modem (MPSS) | `qcom,mss@4080000`, compatible **`qcom,pil-tz-generic`**, `qcom,pas-id = 4`, `firmware-name = "modem"`, `memory-region = pil_modem_mem` (0x8b000000, 0x8400000), `qcom,sysmon-id 0`, `qcom,ssctl-instance-id 0x12`, `qcom,signal-aop`, `pil-modem` bus | live.dts:5908-5945, 20478 [V] |
| IPA | `ipa_smmu_wlan` presente | live.dts:6397 [V] |
| Firmware nella partizione `modem` (FAT, solo nomi/dimensioni) | `IMAGE/WLANMDSP.MBN` (3 960 532 B); `IMAGE/BDWLAN.*` 26 328 B ciascuno in ~40 varianti (`.102`, `.104`…`.109`, `.10E`, `.10F`, `.202`, `.B04`…`.B38`, `.BIN`); `IMAGE/BDF_J20G.BIN`, `IMAGE/BDF_J20I.BIN` (26 328 B); `IMAGE/REGDB.BIN` (19 348 B); `IMAGE/MODEM.MDT` 9100 B + `MODEM.B*` | `backup/surya/modem.img`, listato in sola lettura [V] |
| Altri | `modemst1`/`modemst2` (2 MiB ciascuno) per rmtfs | `backup/surya/` [V] |

**Differenze rispetto al Poco F1 (SDM845)** [V DTB vs `docs/WIFI_BT_PLAN.md`]:

| Aspetto | Poco F1 | surya (SM7150) |
|---|---|---|
| Avvio del modem | **MBA** + auto-autenticazione (`qcom,pil-self-auth`, RMB, hyp-assign) | **PAS** (`pil-tz-generic`, `pas-id 4`): stesso schema dello zap GPU e di ios-topaz (SM6225) |
| Base icnss / WLAN | 0x18800000 | 0x18800000 |
| Stream ID SMMU WLAN | 0x40/0x41 (CE) | **0x240** |
| MSA | 0x8e300000 | **0x97500000** (0x180000) |
| Modem | 0x8e400000/0x7800000 | **0x8b000000/0x8400000** |
| Board data | `bdwlan.*` F1 | `BDWLAN.*` / `BDF_J20x.BIN` (J20 = surya [I]) |

## 2. Cosa sappiamo (verificato)

* **Sul Poco F1 (xnu-12377, iOS 26) il Wi-Fi funziona** fino a: lista di reti reali in Impostazioni (boot 51), **join WPA2-PSK in 251 ms, DHCP e Internet** sul WCN3990 (`NOTES.md` boot 54, patch 1100-1130; commit `f4b36c1`) [V].
  Patch: `1100-1103` (stand-in `sweet_80211`: `en0`, proprietà IOKit per `wifid`, dispatcher `SIOCGA80211/SIOCSA80211`, eventi cinetici), `1104-1123` (modem MBA, rmtfs/TFTP/pd-mapper,
  WLFW, copy engine, HTC/WMI, HTT, prima scansione), `1124-1130` (servizio persistente, scansione reale, join WPA2, EAPOL/TID, cache BSS).
* **iOS 18 sul F1: nessun Wi-Fi.** `docs/WIFI_BT_IOS18.md` §3: mancano i due driver (catena chip + lato iOS); la ripianificazione per iOS 26 (`WIFI_IOS26.md`) mostra che
  **lato iOS** basta un'interfaccia in-kernel con le giuste proprietà IOKit e gli ioctl Apple80211, perché iOS 26 usa un DriverKit dext (`AppleBCMWLAN`) e **ripiega sul path ioctl**.
  Per iOS 18 il percorso lato iOS è da rifare/verificare [?].
* **Sul surya nulla del Wi-Fi è stato provato.** Il telefono parte con iOS 18.7.8 (`NOTES.md` 2026-11-02). L'ADT surya ha solo un `/chosen mac-address-wifi0` sintetico (`tools/mkadt.py:58-80`) e **nessun nodo `/arm-io/wlan`**, quindi `wifid` non parte [V]. Per aggiungerlo esiste `tools/adt-wlan.py` (usato per iOS 26 sul F1) [V].
* Il modem va avviato prima: servono SMEM, SMP2P, GLINK, QRTR, i tre servizi host (pd-mapper, tftp, rmtfs) e il handshake WLFW (QMI 0x45): `docs/WIFI_BT_PLAN.md` §1.2-1.5 [V]. Le tabelle sono SoC-indipendenti per il WCN3990 (4 vdev, 33 peer).
* Il lavoro PAS di ios-topaz (0113) non è stato importato (SM6225); la nostra pila PAS GPU (`sweet_scm.c`, INIT_IMAGE/MEM_SETUP/AUTH_AND_RESET/SHUTDOWN status 0 sullo zap) è già provata sul surya [V per lo zap].

## 3. Ipotesi di implementazione su Heartwood/XNU + iOS, a fasi

| Fase | Obiettivo | Come | Rischio |
|---|---|---|---|
| W0 | Mappa e sola lettura | stesso insieme di `sweetwlan=1`/`=3` del F1 (probe, inventario `modem`), con indirizzi surya; lettura FAT16/32 della partizione `modem` (`sweet_ufs_read_part`) | basso |
| W1 | Avviare l'MPSS con PAS | riusare il codice PAS della GPU per `pas-id 4`: mdt+b00..bNN da `IMAGE/MODEM.MDT`, mapping in 0x8b000000, SMEM walk + SMP2P, voti RPMh per CX/MSS | **il passo più rischioso** (reset senza panic) |
| W2 | GLINK + QRTR | edge `modem` (pid 1) su SMEM; `1113-beryllium-mpss-rmtfs-qrtr-glink` come modello (cambiano doorbell/IRQ per SM7150) | medio |
| W3 | Servizi host | pd-mapper, tftp (`IMAGE/` dalla partizione), rmtfs (`modemst1/2`, scritture scartate); `hyp-assign` per MSA 0x97500000 | medio |
| W4 | WLFW | QMI WLFW: `ind_register`, `host_cap` (saltarla se è un quirk), MSA info, `bdf` da `BDWLAN.*`/`BDF_J20x.BIN`, cal, `wlan_mode`; stream ID SMMU **0x240** | medio |
| W5 | Dati | CE, HTC/WMI, HTT, vdev di stazione; scansione passiva → attiva | medio-alto |
| W6 | Lato iOS | `en0` + proprietà IOKit + ioctl Apple80211 (`sweet_80211`), nodo ADT `/arm-io/wlan` (`device_type = wlan`, `local-mac-address`) | su iOS 18 da verificare; su iOS 26 già pronto sul F1 |
| W7 | Join WPA2/WPA3, regdomain, cache BSS | come 1126-1130 | basso-medio |

**Ordine consigliato per il surya** [I]: terminare prima la GPU (stessi strumenti PAS/SMMU), poi W0-W2.

## 4. Ostacoli e rischi

1. **Reset del SoC** per accessi non autorizzati (XPU), SMMU non configurata o PAS errato: nei dati del F1 un SMR sbagliato o un accesso a blocchi spenti resettava il telefono senza panic (`docs/WIFI_BT_IOS18.md` §3, `GPU_A630_BRINGUP.md`).
2. **Il modem è un sistema grande.** Se il modem non parte pulito, il Wi-Fi non esiste: la dipendenza è la stessa di `rmtfs`, `pd-mapper`, `tftp` (3 demoni host). Le scritture rmtfs vanno scartate (non toccare `modemst1/2` nel telefono) [I].
3. **Scelta del file `BDWLAN`.** Esistono ~40 varianti: la scelta dipende da board-id/chip rev: ath10k fa lookup per `bus=snoc,qmi-board-id=...`; qui servirà leggere l'ID dal modem o dai file `BDF_J20G/I` [?].
4. **Regdomain** (`REGDB.BIN`, 19 348 B): paese e canali; il F1 ha usato un elenco canali minimo (`1103`) [V].
5. **MAC Wi-Fi.** Su F1 i MAC stavano in `persist/wlan_bt/`; su surya il percorso è da verificare [?]; il nostro ADT oggi mette un MAC sintetico (`tools/mkadt.py`, board surya) [V].
6. **iOS 18 vs iOS 26.** Il lavoro riuscito del F1 è su xnu-12377. La serie per iOS 26 (`xnu/patches-12377`) va portata su surya; iOS 18 (xnu-11215) ha un Wi-Fi diverso lato framework [?].
7. **Firmware non redistribuibile** (Qualcomm/Xiaomi): niente in git, solo nomi e dimensioni.
8. **Carico CPU**: l'iOS senza GPU è già lento; il WLAN service aggiunge thread kernel.

## 5. Lavoro della community e riferimenti utili

* **ry-diffusion/Bergamota (F1):** la pila completa fino a WPA2+DHCP+Internet su iOS 26; documenti `WIFI_IOS26*.md`, `WIFI_BT_PLAN.md`; patch 1100-1130 [V, già nel nostro branch].
* **MakrSas/ios-topaz (SM6225, PAS):** `0113-0126` (PAS boot, GLINK/QRTR, host services, WLFW, scansione); senza licenza: solo letto e descritto (`docs/TOPAZ_MERGE.md`).
* **Linux:** `ath10k_snoc` (`qcom,wcn3990-wifi`), `qcom_q6v5_pas` (modem su SM7150), `qcom,wcn3990-wifi` DT [V come riferimento].
* **itlwm / AirportItlwm:** modello di un driver IO80211 in-kernel (`docs/WIFI_KEXT_STUDY.md`).
* Nessun Wi-Fi su surya funzionante noto nei fork censiti il 2026-11-02 [V].

## 6. Domande aperte / prossimi passi

1. `qcom,mss@4080000` si avvia con PAS 4 senza passare per l'MBA? Sulle SM7150 sì per il DTB; lo dobbiamo provare [?].
2. Quale `BDWLAN.*` serve al nostro hardware (J20G o J20I)? [?]
3. Dove sta il MAC Wi-Fi su surya? [?]
4. Il nodo `/arm-io/wlan` va aggiunto a `tools/mkadt.py` (board surya): parametri e `wlan.*` da iOS 18 [?].
5. iOS 18: quale interfaccia vuole `wifid`/IO80211 se c'è il dext? Verificare nel root iOS 18.7.8 già installato [?].

## 7. Stato

| Fase | Stato | Data |
|---|---|---|
| W0 mappa dal DTB | in questo documento | 2026-11-02 |
| W1 MPSS via PAS | da fare | |
| W2-W5 chip | da fare | |
| W6 lato iOS | da fare | |
| W7 join | da fare | |
