# Adreno 618 (surya): servizio IOKit e user client (patch 0320)

Stato: **scritto, mai compilato in CI, mai eseguito su hardware**. Il type-check locale e' stato fatto solo con header stub (`clang++ -fsyntax-only -Wall -Wextra -Werror`), quindi le API XNU reali sono verificate solo per analogia con 0182..0217 (compilate in CI sul Poco F1).

## 1. Cosa e' stato aggiunto

Patch `xnu/patches-11215/0320-surya-gpu-adreno-iokit-user-client.patch` (si applica dopo 0301/0318):

| File | Contenuto |
|---|---|
| `iokit/Kernel/arm/SweetAdreno618.cpp` | classi `SweetAdreno618` (IOService) e `SweetAdreno618UserClient`, funzioni `SweetAdreno618Register/Publish` |
| `iokit/Kernel/arm/SweetAdreno618ABI.h` | copia **byte per byte identica** a `xnu/gpu-abi/sweet_adreno_abi.h` (verificata con `cmp` sull'albero di prova) |
| `iokit/conf/files.arm64` | una riga `SweetAdreno618.cpp optional iokitcpp` dopo `SweetAdreno.cpp` |

Il kernel e' lo stesso del Poco F1: `SweetAdreno630`, `SweetAdrenoUserClient` e `AGXDeviceUserClient` (patch 0182/0184/0214...) esistono nello stesso binario, quindi nomi di classe, file e tipi interni (`a618_gem` invece di `sweet_gem`) sono nuovi.

## 2. Pubblicazione e gating

* Nessun comportamento cambia se non succedono **tutte** queste condizioni: `sweet_board_surya()` (0300), `sweet_a618_ready()` vero, boot-arg `sweetgpu=11`.
* Il nucleo (0301, `bsd/dev/sweet_gpu.c`) chiama `sweet_a618_ready_hook` una volta quando e' pronto. Il puntatore e' installato da `SweetAdreno618Register()`, invocata dal costruttore statico `gSweetAdreno618Registrar` (stesso meccanismo dei costruttori delle metaclassi OSMetaClass, quindi eseguito al caricamento del runtime C++ del kernel, ben prima che il bring-up della GPU finisca). **Ipotesi non verificata**: l'ordine reale dei costruttori statici rispetto al bring-up; se il nucleo fosse gia' pronto al momento del Register, il hook non verrebbe chiamato. Alternativa gia' pronta: il nucleo puo' chiamare esplicitamente `SweetAdreno618Register()` (idempotente, `extern "C"`) e poi `SweetAdreno618Publish()` da un contesto in cui IOKit e' attivo.
* `SweetAdreno618Publish()` e' idempotente (`gA618Published`), alloca l'IOLock, crea il servizio, lo attacca alla piattaforma (`IOService::getPlatform()`) e fa `registerService()`. Se la piattaforma non c'e' ancora logga e non marca come pubblicato (un secondo invocazione puo' riprovare).
* Da `-Wglobal-constructors` ci si aspetta nessun problema (le metaclassi IOKit generano gia' costruttori globali); se il CI lo segnalasse, sostituire l'oggetto statico con una chiamata esplicita (vedi sopra).

### Proprieta' di registro del servizio

| Proprieta' | Valore |
|---|---|
| nome (`setName`) | `SweetAdreno618` (`SWEET_GPU_SERVICE_NAME_A618`) |
| `IOUserClientClass` | `SweetAdreno618UserClient` |
| `SweetGPUABIVersion` | 1 (`SWEET_GPU_ABI_VERSION`, invariato) |
| `SweetGPUChipID` | `0x06010800` |

## 3. ABI

Aggiunte minime a `xnu/gpu-abi/sweet_adreno_abi.h` (nessun selettore ne' layout cambiato): `SWEET_GPU_SERVICE_NAME_A618`, `SWEET_GPU_ID_A618` (618), `SWEET_GPU_CHIP_ID_A618` e un commento. La costante `SWEET_GPU_ABI_VERSION` resta 1 e **non** c'e' nessun nuovo `abi_minor`: il campo `abi_minor` di `sweet_gpu_info` descrive `SURFACE_BO` (minor 1/2), assente su surya, quindi `INFO` risponde `abi_minor = 0` e senza `SWEET_GPU_INFO_CAP_SBO_ALIAS`. Un client userland distingue il servizio dal nome (`SweetAdreno618`) e da `chip_id`.

## 4. Selettori

| Selettore | Stato su surya |
|---|---|
| 0x40 GET_PARAM | funziona (`sweet_a618_param`; i numeri sono i `MSM_PARAM_*` come sul F1: GPU_ID 618, CHIP_ID 0x06010800, GMEM_SIZE 0x80000, GMEM_BASE 0x100000, VA_START 0x80000000, VA_SIZE 0x80000000; altri valori in base al nucleo, altrimenti `kIOReturnUnsupported`) |
| 0x41 SET_PARAM, 0x4c SUBMITQUEUE_QUERY | `kIOReturnUnsupported` |
| 0x42 GEM_NEW | funziona: buffer write-combined (`IOBufferMemoryDescriptor`, `kIOMapWriteCombineCache`), azzerato e `sweet_a618_cache_sync` prima dell'uso, una PTE per 4 KiB tramite `sweet_a618_vm_map`; dimensione arrotondata a 16 KiB (`round_page_64`, come sul F1), max 256 MiB, 4096 BO per connessione |
| 0x43 GEM_INFO | GET_OFFSET (= handle, usato come `memoryType` di `IOConnectMapMemory64`), GET_IOVA, GET_FLAGS, SET_NAME (ignorato), SET_IOVA (prima del primo submit) |
| 0x44/0x45 CPU_PREP/FINI | funziona (timeout relativo; FINI e' un `dsb sy`) |
| 0x46 GEM_SUBMIT | funziona: cmd BUF e CTX_RESTORE_BUF come IB, PRIMARY non supportato, reloc applicate all'alias kernel del BO comandi; niente fence fd/syncobj. Aggiunto un `dsb sy` dopo le reloc |
| 0x47 WAIT_FENCE | funziona, timeout **relativo**; attesa >= 1 s e ancora incompleta dopo 5 s: GPU dichiarata persa (`sweet_a618_mark_lost`); chiama `sweet_a618_report("wait timed out")` come 0184 |
| 0x48 MADVISE | stub (`retained = 1`) |
| 0x09 GEM_CLOSE | funziona; attende l'ultima fence (2 s), poi unmap/free; se la GPU non finisce il BO e' **lasciato trapelare** apposta |
| 0x4a/0x4b SUBMITQUEUE_NEW/CLOSE | funziona (id 1..63, prio 0) |
| 0x70 INFO | funziona anche a GPU persa (flags bit0 pronta, bit1 persa) |
| 0x72 BO_SYNC | funziona (`sweet_a618_cache_sync` sull'alias kernel) |
| 0x71 SURFACE_BO | `kIOReturnUnsupported` (**TODO**: import IOSurface come 0187/0214/0217) |
| 0x73 DEBUG | `kIOReturnUnsupported` (**TODO**: snapshot registri, serve una funzione `sweet_a618_debug` nel nucleo) |

Alla chiusura della connessione (`clientClose`) tutti i BO vengono rilasciati con la stessa regola (attesa ultima fence, leak se la GPU e' appesa).

## 5. Differenze rispetto a 0182 (+0184..0217)

* Chiamate `sweet_gpu_*` -> `sweet_a618_*` (contratto congelato con la fase 1); un solo `IOLock` globale (`gA618Lock`), nessun `sweet_gpu_lock/unlock` ne' epoch ne' `vm_flush`/`check_fault` (il contratto non li prevede: il TLB flush dopo map/unmap e' responsabilita' del nucleo).
* Granularita' 4 KiB (`GPU_PAGE`) come 0185, con in piu' il controllo di allineamento anche in SET_IOVA.
* `WAIT_FENCE` con GPU non pronta ritorna `kIOReturnIOError` (come 0185).
* Non portati: pid dei client e metriche `kern.sweet_gpu` (0186), SURFACE_BO e alias scrivibili (0187/0214/0217), DEBUG (0184), governor (0188), recovery/epoch (0186).
* **Nome di classe `AGXDeviceUserClient` (boot-arg `sweetgpuuc`): non portato.** La classe esiste gia' nel kernel (F1) e non puo' essere definita due volte; la proprieta' `IOUserClientClass` e' sempre `SweetAdreno618UserClient`. Se il sandbox iOS richiedesse quel nome, serve una soluzione diversa (es. un solo UC condiviso con scelta per scheda).
* Il sandbox iOS deve consentire `iokit-open` per `SweetAdreno618` e la classe `SweetAdreno618UserClient`.

## 6. Verifiche eseguite

* `patch -p1` della patch 0320 su un albero vuoto per i due file nuovi: applicati senza errori (la parte `files.arm64` richiede il file esistente).
* `files.arm64`: ricostruito lo stato post-0182 (unica patch successiva a toccarlo; le altre che lo toccano sono tutte < 0182) dal contesto dell'hunk di 0182, applicato prima l'hunk 0182 e poi quello di 0320: OK, riga inserita dopo `SweetAdreno.cpp`.
* `cmp` fra `SweetAdreno618ABI.h` estratto e `xnu/gpu-abi/sweet_adreno_abi.h`: identici; idem per il `.cpp`.
* `clang++ -fsyntax-only -Wall -Wextra -Werror -fno-exceptions -fno-rtti` contro header stub: nessun errore/avviso.

## 7. Rischi e non verificato

1. Tutte le API IOKit sono quelle usate da 0182 (compilata in CI su 11215); le differenze sono solo rinomine e rimozioni. Non c'e' compilazione reale fino al prossimo CI.
2. Ordine dei costruttori statici vs `ready` (sezione 2): inferenza.
3. Include `../../../bsd/dev/sweet_board.h` come fanno altri file iokit/Kernel/arm (0097, 0123, 0126).
4. Il contratto con il nucleo non specifica se `sweet_a618_param` ritorna valori per tutti i `MSM_PARAM_*` (4 MAX_FREQ, 5 TIMESTAMP...): cio' che non e' risposto torna `kIOReturnUnsupported`.
5. Single tenant, nessuna isolazione, GPU persa = riavvio (invariato rispetto al F1, vedi `docs/GPU_KERNEL.md` sezione 7).

## 8. Provenienza

Il design deriva dal user client del Poco F1 di questo repository (a sua volta derivato come forma da `hackios-xnu-kernel-ext`, APSL-2.0, solo lettura dell'uso API; nota di derivazione in `docs/GPU_KERNEL.md` sezione 10). I selettori e le strutture sono l'ABI del driver Linux msm (`include/uapi/drm/msm_drm.h`, con la eccezione syscall-note dell'UAPI), ridichiarati con nomi `sweet_`. Nessuna fonte esterna aggiuntiva e' stata consultata per questa patch.
