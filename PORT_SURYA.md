# Port di Bergamota su Poco X3 NFC (surya) — piano a fasi

Obiettivo: (1) port **semplice** per vedere cosa gira; (2) progetto separato: **accelerazione 3D**.

Legenda affidabilità: **[V]** letto nel repo/docs · **[I]** mia inferenza · **[?]** da verificare sul tuo telefono.
Nulla è stato eseguito su hardware: ho solo clonato il repo e letto codice e documentazione.

---

## 1. Buone notizie (correggo la stima del primo messaggio)

- **Il repo ha già una board "sweet" = SM7150**, lo stesso SoC del tuo X3 (Snapdragon 732G). Nasce dal fork di ios-sweet (Redmi Note 10 Pro). [V] `enxerto/board.h`, `tools/mkadt.py`
- Il bootloader Enxerto usa `sweet` come **board di default**; `beryllium` (Poco F1) è la variante aggiunta dopo. [V] `enxerto/board.h`
- Valori **identici tra SM7150 e SDM845** nel codice: PS_HOLD, GIC, UFS (0x1d84000). [V] `enxerto/board.h`
- `mkadt.py` ha già per `sweet`: CPU `qcom,kryo470` (6× A55 + 2× A76), frequenze 1.8/2.3 GHz, display 1080×2400. [V] `tools/mkadt.py`
- Le patch XNU `*-sweet-*` (116 su 219) sono la base per **SM7150** (UFS, USB, SMP, root HFS, AMFI…). Quelle `beryllium-*` (89) non ti servono. Alcune `sweet` restano legate alla scheda del Redmi (touch, batteria). [V] `xnu/patches-11215/`
- **Adreno 618 (il tuo X3) è già stato fatto avanzare** in altri fork: GMU, HFI, zap shader via PAS, buffer CP. Il Poco F1 (Adreno 630) ha dovuto adattarlo da lì, non il contrario. [V] `docs/GPU_RESEARCH.md`, `docs/GPU_A630_BRINGUP.md`

Conclusione: il tuo X3 è **più vicino** al codice di base di quanto lo sia il F1. Il lavoro è adattare una board, non scriverne una da zero.

---

## 2. Cosa cambia tra "sweet" e il tuo X3 (surya)

Stesso SoC ≠ stessa scheda. Cosa va controllato:

| Area | Stato | Dove si decide |
|---|---|---|
| GIC, UFS, PS_HOLD, timer | Uguali (SoC) [V] | — |
| CPU/frequenze | Uguali (Kryo 470) [I] | `mkadt.py` |
| Framebuffer (indirizzo splash, 1080×2400) | **[?]** | DTB live (`cont_splash_region`) |
| Registro IMEM restart reason | **[?]** | DTB live |
| Pannello + backlight | **Probabilmente diversi** [?] | DTB live, `docs/DISPLAY_HW.md` |
| Touch (controller, bus I²C/GENI) | **Probabilmente diverso** [?] | DTB live; patch 0060/0063 sono per il touch di sweet |
| Tasti, PMIC, batteria/charger | **[?]** | DTB live, patch 0062 |
| USB (DWC3 a 0xa600000) | Uguale come SoC [I]; SMMU/PHY **[?]** | patch 0072–0081 |
| RAM e carve-out | **[?]** (X3 ha 6/8 GB) | memory node del DTB live |
| Wi-Fi/BT/audio/camera | Non funzionano nemmeno su F1 [V] | fuori perimetro |

---

## 3. Piano a fasi — port semplice

### Fase 0 — Sicurezza (prima di toccare qualunque cosa)
- Backup **completo** delle partizioni del telefono. Il repo lo fa con 79 partizioni, tutte tranne userdata/system/cache. [V] `NOTES.md`
- Decidi **dove sta ora postmarketOS**: l'immagine iOS va in `userdata` e **cancella** quello che c'è. [V] `AGENTS.md`
- Per i test usa solo `fastboot boot` (immagine temporanea, nessun flash). [V] `NOTES.md`
- Dopo un flash grosso l'ABL Xiaomi si blocca: serve rientrare in fastboot a mano (Power + Vol−). Mai `fastboot getvar`. [V] `NOTES.md`

### Fase 1 — Raccolta dati dal telefono
- Servono: **boot.img stock** (ROM Android), **FDT live** del telefono, cmdline. [V] `tools/extract-dtb.py`
- Il repo legge l'FDT live in recovery con `adb pull /sys/firmware/fdt live.dtb`. [V] `tools/extract-dtb.py`
- Attenzione: dev'essere l'FDT passato dall'**ABL Android**, non quello mainline di postmarketOS. [I]
- `extract-dtb.py BOOT.img dtb/surya --live live.dtb` sceglie il blob giusto per msm-id e board-id. [V]

### Fase 2 — Enxerto (bootloader) su surya
- `enxerto/board.h`: nuovo blocco `BOARD_SURYA` (FB_BASE, FB_W/H, IMEM_RESTART_REASON dal DTB live).
- `enxerto/build.sh`: nuovo `case surya`, con DTB in `dtb/surya/dtb`. L'header del boot.img (v0/v1/v2) va copiato dallo stock. [?]
- **Criterio di successo:** `fastboot boot` mostra la schermata di Enxerto (logo + fastboot Bergamota).

### Fase 3 — Kernel Heartwood con ADT di surya
- `tools/mkadt.py`: nuova voce `BOARDS['surya']`. Il `compat` deve includere `xiaomi,sweet`, perché la `SweetPlatformExpert` lo cerca. [V] commento di `mkadt.py` per beryllium
- Kernel: GitHub Actions su runner macOS (`xnu.yml`), serve un tuo fork del repo. [V] `README.md`
- **Criterio di successo:** log del kernel XNU sul framebuffer, poi USB ECM con SSH su `root@192.168.7.2`.

### Fase 4 — Root iOS 18.7.8
- `renegade fetch` + `renegade root`: ~20 GB liberi, nessun sudo. [V] `README.md`
- Flash su `userdata` (**distruttivo**), conferma esplicita prima. [V] `AGENTS.md`
- **Criterio di successo:** SpringBoard sullo schermo (anche lento, anche solo CPU).

### Fase 5 — Periferiche base
- Touch, tasti, display on/off, backlight, batteria. Una patch per volta, dal DTB live.
- Il touch del tuo X3 è la cosa più probabile che manchi. [?]

**Perimetro volutamente escluso:** Wi-Fi, cellulare, Bluetooth, audio, fotocamera, sensori. Non funzionano nemmeno sul F1. [V] `README.md`

---

## 4. Progetto 3D (fase separata, a ricerca)

Stato attuale: nel repo **la GPU non funziona**, nemmeno sul F1; iOS usa rendering CPU. [V] `README.md`

Catena di lavoro dal repo (adattata a Adreno 618):

| Passo | Cosa | Riferimento | Stima del repo (per Adreno 630, dev singolo) |
|---|---|---|---|
| A | GPU accesa: clock, GMU, HFI, SMMU, zap shader (PAS 13), anello CP | `docs/GPU_A630_BRINGUP.md`; fork A618 `mxtpasontop/ios-sweet` ramo `gpu` | 3–8 settimane |
| B | User client IOKit + Turnip (Mesa) con backend Darwin | `gpu/freedreno-sweet/`, `docs/GPU_RESEARCH.md` | 3–6 settimane |
| C | Metal sopra Vulkan (metal2vulkan) | `docs/GPU_RESEARCH.md` §3 | 2–4 settimane |
| D | CoreAnimation compone su GPU | `docs/GPU_RESEARCH.md` §6 | 2–6+ mesi, rischio alto |

Scorciatoia con ritorno visibile: accelerare solo blit/composizione nel kernel (AppleCLCD), senza Metal. Richiede solo il passo A. [V] `docs/GPU_RESEARCH.md`

Cosa cambia per surya rispetto al F1 [I]:
- GPU **Adreno 618** (non 630): tabelle, indirizzi PDC, stream ID SMMU e firmware sono quelli di SM7150, non di SDM845.
- Firmware (GMU, SQE, zap) da **estrarre dal tuo telefono**: lo zap è firmato per dispositivo. [V] `docs/GPU_A630_BRINGUP.md`
- Il fork A618 si è fermato prima di eseguire comandi sul CP, quindi nemmeno lì c'è una prova funzionante. [V] `docs/GPU_A630_BRINGUP.md` rischio 6

---

## 5. Rischi principali

1. **Reset del SoC senza panic** per accessi a blocchi spenti, SMR sbagliati o SMMU: il repo li documenta come il pericolo numero uno.
2. **Touch e pannello sconosciuti** finché non leggiamo il DTB live del tuo telefono.
3. **Host macOS**: l'app Bergamota è SwiftUI e il flash passa da un Mac; da verificare cosa regge il tuo MacBook (Xcode installabile su macOS 13?). [?]
4. **Perdita di postmarketOS** e del progetto handheld: il flash cancella userdata.
5. **GPU a rischio alto**: i tempi sopra sono del repo per il F1, non garanzie.

---

## 6. Cosa mi serve da te per partire

- [ ] Boot.img stock di surya (ROM Android) o conferma che puoi estrarlo
- [ ] `live.dtb` letto dall'ABL Android (recovery + `adb pull /sys/firmware/fdt`)
- [ ] Dove sta installato postmarketOS ora (userdata o altra partizione)
- [ ] Cosa gira sul tuo Mac (versione macOS e Xcode) o se hai un altro Mac
- [ ] Un tuo fork GitHub del repo per le Actions

Con DTB e boot.img preparo il ramo con la board `surya` (board.h, build.sh, mkadt.py) e un controllo dei valori.

---

## Fonti

- Repo: https://github.com/ry-diffusion/Bergamota (letti: `README.md`, `AGENTS.md`, `NOTES.md`, `enxerto/board.h`, `enxerto/build.sh`, `tools/mkadt.py`, `tools/extract-dtb.py`, `docs/GPU_RESEARCH.md`, `docs/GPU_A630_BRINGUP.md`, `docs/GPU_PHASE0_DISCOVERY.md`, `docs/FORKS_SURVEY.md`, `docs/DISPLAY_HW.md`)
- Supporto SM7150 e Poco X3 NFC in postmarketOS: https://gitlab.com/postmarketOS/pmaports/-/merge_requests/3706
