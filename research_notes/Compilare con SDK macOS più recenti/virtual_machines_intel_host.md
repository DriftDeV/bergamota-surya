# Virtualizing newer macOS guests on an old Intel Mac host (MacBookPro14,3, macOS 13.7.8) for headless compiling

Research date: 2026-11-02. Method: web search plus page fetches. Some primary pages (Broadcom techdocs for Fusion 25H2/26H1, Broadcom KB 315597) were not retrievable in full, so several Fusion version/host-requirement facts rest on mirrors or forum posts; these are flagged. Nothing here was tested on the actual machine.

## 1. VMware Fusion: guest and host support, current version, Ventura compatibility

### Takeaway
On a Ventura 13.7.8 host the newest Fusion that runs is the 13.6.x line (13.6.4, build 24832108, July 2025). Its release notes officially add a macOS Sequoia (15) guest on Intel chipsets. A macOS Tahoe (26) guest is officially supported only from Fusion 25H2 (Oct 2025), which a mirror of the release notes says needs host macOS 15.0 or later, so it will not run on Ventura. Fusion is free for commercial, educational and personal use, but downloads need a (free) Broadcom portal login.

### Cited Findings
- Fusion 13.6 release notes list new guest OS support on the Intel chipset: macOS Sequoia (also Windows Server 2025, Windows 11 23H2, Ubuntu 24.04, Fedora 40). Apple silicon gets no macOS guest. No macOS-guest known issues are listed; the listed issues concern macOS hosts (e.g. NAT/DNS on Sequoia 15.4+). — [Fusion 13.6 release notes (Broadcom techdocs)](https://techdocs.broadcom.com/us/en/vmware-cis/desktop-hypervisors/fusion-pro/13-0/release-notes/vmware-fusion-136-release-notes.html)
- Fusion 13.6.4 is "Build 24832108, 15 JUL 2025"; the 13.6.x download carries the system requirement "macOS Ventura 13.0 and later" and supports both Intel and Apple silicon Macs (Chinese mirror of the release notes, secondary). — [sysin.org mirror via Aliyun](https://developer.aliyun.com/article/1671908)
- Broadcom's KB "Supported host operating systems for Fusion" has a table with a Ventura 13.0.x (Intel) row marked supported for the 13.x, 13.5, 13.6 and 13.6.x columns (column mapping was inferred from the extracted text, so verify in the KB). This table is about host OS, not guest OS. — [Broadcom KB 315635](https://knowledge.broadcom.com/external/article/315635/supported-host-operating-systems-for-vmw.html)
- Wikipedia's version table lists 13.6.4 as the latest Fusion for hosts macOS 13 to 15 (secondary). — [Wikipedia: VMware Fusion](https://en.wikipedia.org/wiki/VMware_Fusion)
- Fusion 25H2 (14 Oct 2025, build 24995814) adds macOS Tahoe as a host on Intel and Apple silicon, and macOS Tahoe as a guest "on Intel systems only". — [VMware blog, 25H2 announcement](https://blogs.vmware.com/cloud-foundation/2025/10/14/vmware-workstation-fusion-25h2-embracing-calendar-versioning-and-new-features/); [Broadcom community 25H2 thread](https://community.broadcom.com/vmware-cloud-foundation/discussion/fusion-25h2-is-released-and-available-now)
- A mirror of the 25H2 release notes lists the system requirement as "macOS Sequoia 15.0 and later" (not confirmed against Broadcom's own page). — [sysin.org mirror via Aliyun](https://developer.aliyun.com/article/1685070)
- Broadcom forum, 19 Jun 2025: "Fusion 13.5.x isn't tested or qualified to run on Sequoia" (13.6.3+ needed for a Sequoia host). The same poster says Tahoe is the last macOS for Intel Macs and warns existing Intel-created VMs will not run on Apple silicon. — [Broadcom community: "path forward from Ventura"](https://community.broadcom.com/vmware-cloud-foundation/discussion/path-forward-from-ventura-1376f)
- Fusion 26H1 was announced 14 May 2026; the blog lists only new Linux/FreeBSD guests (no macOS items) and says both products "remain free for commercial, educational, and personal use". — [VMware blog, 26H1](https://blogs.vmware.com/cloud-foundation/2026/05/14/announcing-vmware-workstation-and-fusion-26h1/)
- 26H1 minimum host: a Russian aggregator says "macOS 12 and later"; a Chinese report says Intel and Apple silicon are supported; neither is authoritative and they conflict with the Sequoia 15.0 line for 25H2. — [comss.ru](https://www.comss.ru/page.php?id=2846); [ithome.com](https://www.ithome.com/0/951/172.htm)
- Licensing: Broadcom KB 368667 says that from 11 Nov 2024 Fusion Pro and Workstation Pro are free for commercial, educational and personal use, for Fusion 13.5.2 and above. A May 2025 forum poster still asked for clearer license text. Download is via the Broadcom support portal (free account). — [Broadcom KB 368667](https://knowledge.broadcom.com/external/article/368667); [Broadcom community](https://community.broadcom.com/vmware-cloud-foundation/discussion/vmware-fusion-pro-license)
- A forum reply states Fusion on Apple silicon does not support any macOS guest (not relevant for this Intel host). — [Broadcom community](https://community.broadcom.com/vmware-cloud-foundation/discussion/could-vmware-fusion-13-create-macos-guest-the-host-is-silicon-mac)
- A user reports (Jun 2025) that a VMware Communities answer says Broadcom dropped all macOS guests from the Fusion 13 guest compatibility list, while macOS guests still run on Intel up to Ventura and "reports that Sonoma will work too" (secondary, conflicts with the 13.6 notes above which add Sequoia). — search result summary of [VMware Communities thread](https://wbsdv95928.lithium.com/t5/VMware-Fusion-Discussions/Can-any-version-of-Fusion-13-run-a-Guest-VM-running-Mojave-OS-on/m-p/2984057) (page not fetched)
- Search snippet (not verified; page returned HTTP 403): VMware's guest-OS page for macOS 26 includes installation steps "for Fusion 13.0 and later". — [VMware GOSIG macOS 26](https://partnerweb.vmware.com/GOSIG/MacOS_26.html)
- Fusion 25H2/Tahoe host issue: with VoiceOver enabled in the guest, VMs crash at startup (Tahoe host). Not relevant on a Ventura host. — [Broadcom community 25H2 thread](https://community.broadcom.com/vmware-cloud-foundation/discussion/fusion-25h2-is-released-and-available-now)

### Inferences
- A Ventura host stays on Fusion 13.6.x. Sonoma (14.x) and Sequoia (15.x) guests are the realistic targets; Sequoia is officially listed for Intel in the 13.6 notes, Sonoma is plausible but I found no explicit statement for it.
- Xcode 16.2 (needs Sonoma 14.5+) and Xcode 26.0 to 26.3 (need Sequoia 15.6+) fit a Sequoia guest on Fusion 13.6.x. Xcode 26.4+ needs Tahoe 26.2+, which would require Fusion 25H2 or later and therefore a host newer than Ventura; this is unreachable here without upgrading the host OS. The host model likely cannot run Sequoia/Tahoe at all (Ventura is stated as its last macOS in the task brief), so Tahoe-only Xcode is effectively out of reach by VM on this machine unless Tahoe guests boot unofficially on 13.6.x (untested, no evidence found).
- A Sequoia guest newer than the host is not blocked by any Fusion statement I found; the real constraint is the Fusion version/host pairing.

### Gaps
- No primary-source confirmation of Fusion 25H2/26H1 minimum host macOS (techdocs not retrievable).
- No report found of a Tahoe guest running on Fusion 13.6.x.
- No report found specifically of a Sonoma/Sequoia guest on a Ventura host with Fusion 13.6.x (found reports on Sonoma/Sequoia hosts and Monterey hosts only).
- Could not retrieve Broadcom KB 315597 content for 13.6/25H2 (it only showed Fusion 10 to 12.2).

## 2. Parallels Desktop (Intel build): macOS guests, cost

### Takeaway
Parallels Desktop 26 supports Ventura 13.7 hosts and Intel Macs, and its requirements page lists old Intel macOS guests up to Sequoia 15.0 subject to host support, but users report macOS 26 guests are broken on Intel hosts. Parallels 27 drops Intel Macs. Cost about $99.99/yr Standard, $119.99/yr Pro.

### Cited Findings
- PD 26 requirements: host list includes Tahoe 26.0, Sequoia 15.5, Sonoma 14.7 and Ventura 13.7; Intel-host guest list includes macOS Sequoia 15.0 down to Mavericks, "only x86 versions are supported", and a footnote says macOS guests require a Mac that supports that OS as a host. (fetched extraction; the summarizer paraphrased the table) — [Parallels KB 124223](https://kb.parallels.com/en/124223)
- Parallels says PD 26 fully supports Intel Macs; PD 27 (built for macOS 27, Apple silicon only) cannot be installed on Intel Macs. — search result summary of Parallels pages ([kb.parallels.com/en/131175](https://kb.parallels.com/en/131175))
- Parallels forum, Jul to Oct 2025 (user posts, no Parallels staff reply): macOS 26 beta guest on a macOS 15.5 Intel host was "very slow, badly broken UI and very unstable"; Safari crashed with a graphics-driver error; after upgrading to PD 26 with macOS 26.0.1 guest "things have not improved". — [Parallels forum thread](https://forum.parallels.cn/threads/macos-26-beta-intel-guest-on-macos-15-5-intel-host.367863/)
- Price (secondary, may have changed): Standard $99.99/yr, Pro $119.99/yr, Standard perpetual $219.99; promotions of 35 to 45% seen. — search result summary of [AppleInsider](https://appleinsider.com/articles/25/09/12/save-35-on-parallels-desktop-26-to-run-windows-apps-on-macos-tahoe) and others (not individually fetched)

### Inferences
- Parallels offers no advantage over free Fusion 13.6.x for this use: it costs money, and its Intel macOS guest ceiling is Sequoia 15.0 per the requirements page.
- The Intel guest list reading "up to Sequoia 15.0" comes from a summarized page; I did not see a statement about 15.x point releases or 14.x specifically.

### Gaps
- No Parallels-staff statement on macOS 14/15 guests on Intel hosts running Ventura.
- Current exact price not checked on parallels.com.

## 3. UTM/QEMU (HVF), OSX-KVM, VirtualBox, Apple Virtualization.framework

### Takeaway
Virtualization.framework macOS guests are Apple-silicon only per Apple and UTM docs, so it is not an option on this Intel host. OSX-KVM is a Linux/KVM project and is not documented for macOS hosts. QEMU's HVF accelerator works on Intel Macs for same-architecture guests, but I found no evidence of a working macOS-on-macOS QEMU/HVF Sonoma+ setup. VirtualBox has no usable story found.

### Cited Findings
- Apple: "Virtualize macOS on a Mac — Configure and run macOS guests on Apple silicon"; the platform configuration "for booting macOS on Apple silicon"; VZMacOSRestoreImage exists from macOS 12.0. — [Apple docs](https://developer.apple.com/documentation/virtualization/virtualize-macos-on-a-mac.md)
- UTM docs: macOS guests are supported on "Apple Silicon hosts running macOS 12 or higher"; Intel hosts not mentioned. — [UTM docs](https://docs.getutm.app/guest-support/macos/)
- OSX-KVM (kholia): needs a Linux host, QEMU >= 8.2.2, VT-x/SVM, AVX2 for macOS Ventura and later; installer menu lists High Sierra to Tahoe (Sonoma marked recommended); headless via boot-macOS-headless.sh; VNC/SSH not enabled out of the box; "lacks graphical acceleration, a reliable sound sub-system, USB 3"; README says nothing about macOS hosts. — [OSX-KVM README](https://github.com/kholia/OSX-KVM)
- QEMU HVF on Intel needs VT-x with EPT and unrestricted mode; QEMU code paths dropped macOS older than 12.0; some features are not implemented for HVF and TCG is the fallback; HV_DENIED if the binary lacks the entitlement. — [libvirt hvf patch series](https://lists.libvirt.org/archives/list/devel@lists.libvirt.org/thread/RNWP5CVIIX3D4CST3UAJVAKGQFDEW4I5); [Coastal Carolina QEMU guide](https://ww2.coastal.edu/mmurphy2/materials/qemu/install/macos)
- UTM v4.6.2 release notes mention macOS 15 guest improvements and a fix for slow virtualization on Intel Macs (#6325); unclear which guest types. — [UTM v4.6.2](https://github.com/utmapp/UTM/releases/tag/v4.6.2)
- VirtualBox: only found third-party material; an Apple-silicon VirtualBox cannot run x86 guests; Genymotion says the VirtualBox hypervisor on macOS is for "old Macs with Intel x86_64" and not recommended; Sequoia-guest guides rely on VBoxManage CPUID hacks and target Windows hosts. — [Genymotion macOS install guide](https://rozz.genymotion.com/pages/macos-installation-guide.html); [iBoysoft guide](https://iboysoft.com/howto/install-macos-sequoia-on-windows.html)

### Inferences
- The OSX-KVM/OpenCore route on a macOS host would mean QEMU with HVF instead of KVM. I found no documentation or reports that this works for Ventura+ guests; MSR/CPUID handling that OSX-KVM sets for KVM may not translate (inference, not verified).
- OSX-KVM also requires a Linux host; on this Mac that would mean running Linux inside a VM first (nested virtualization), which is not supported by Fusion on macOS hosts in any source I found (not verified either way).

### Gaps
- No primary or dated report of macOS 14/15 guest working under UTM/QEMU on an Intel Mac host.
- No VirtualBox official page statement on macOS guests on Intel macOS hosts (VirtualBox supports macOS guests only on Apple hardware per its manual, from general knowledge, not confirmed here).

## 4. Apple macOS EULA

### Takeaway
macOS Tahoe's EULA allows up to two additional virtual instances per Apple-branded computer you own or control that already runs macOS, for software development, testing during development, macOS Server, or personal non-commercial use. It explicitly allows "any prior macOS ... or subsequent release", so a newer guest than the host is permitted.

### Cited Findings
- Section 2B(iii): "to install, use and run up to two (2) additional copies or instances of the Apple Software, or any prior macOS or OS X operating system software or subsequent release of the Apple Software, within virtual operating system environments on each Apple-branded computer you own or control that is already running the Apple Software, for purposes of: (a) software development; (b) testing during software development; (c) using macOS Server; or (d) personal, non-commercial use." — [macOS Tahoe SLA PDF (Apple)](https://www.apple.com/legal/sla/docs/macOSTahoe.pdf)
- The grant "does not permit you to use the virtualized copies ... in connection with service bureau, time-sharing, terminal sharing, relay service or other similar types of services"; and you may not use the software to run other Apple OSes (iOS, iPadOS, watchOS, tvOS) in VMs on Macs. — same PDF

### Inferences
- A build VM on the owner's own Mac for compiling one's own software fits (a)/(b). Exposing it to third parties as a build service might trip the "service bureau/time-sharing" clause (inference). Snapshots/clones count as instances; keep to two VMs at once (reading of "two additional copies").
- The guest must run on the Apple-branded Mac (this MacBook Pro qualifies).

### Gaps
- Not checked: the EULA text for the specific older macOS versions (Sonoma/Sequoia) installed in the guest; only the Tahoe SLA was read. I did not find wording tying the guest version to the host version.

## 5. Obtaining installers and Xcode in the guest

### Takeaway
On Ventura the easiest route for a Sonoma/Sequoia installer is mist-cli (Big Sur+; Intel installers as .app/.dmg/.iso/.pkg), with softwareupdate --fetch-full-installer as a second option. Fusion needs the installer put into a bootable disk image via createinstallmedia (the .dmg dragged into the new-VM window). No Apple ID is needed for the macOS installer; xcodes needs an Apple ID for Xcode.

### Cited Findings
- mist-cli: runs on macOS Big Sur 11 and later; commands such as `mist list installer [--compatible]` and `mist download installer "26.0" image` (the README example shows Tahoe); Apple silicon downloads firmware (.ipsw), Intel downloads installers; Apple ID not mentioned. Whether versions newer than the host can be downloaded was not stated (the --compatible flag filters by the host). — [ninxsoft/mist-cli](https://github.com/ninxsoft/mist-cli)
- softwareupdate --fetch-full-installer: reports of "Update not found" failures, e.g. broken on macOS 15.4.x for all versions while working on 15.3.1; `softwareupdate --list-full-installers` shows what Apple offers. — [Apple Developer Forums](https://developer.apple.com/forums/thread/781488); [iBoysoft](https://iboysoft.com/tips/install-failed-with-error-update-not-found.html)
- Fusion 13.6.4 install procedure that worked on Intel (user thread Jul 2025): download the full installer, `hdiutil create` a blank image of 15 GB or more, mount it, run `createinstallmedia --volume` on it, eject, then drag the resulting .dmg directly into the new-VM window (do not convert to CDR/ISO) and select the matching macOS guest type; Sequoia needed a 16 GB image (Sonoma 15 GB). — [Broadcom community thread (2019 MBP)](https://community.broadcom.com/vmware-cloud-foundation/discussion/unable-to-install-macos-guest-on-2019-intel-macbook-reposting-as-discussion); [Broadcom community Sequoia thread](https://community.broadcom.com/vmware-cloud-foundation/discussion/vmware-tools-for-macos-sequoia-150)
- Xcode system requirements (Apple): 16.2/16.1/16.0 need Sonoma 14.5+ (up to Sequoia 15.x); 16.3 Sequoia 15.2+; 16.4 Sequoia 15.3+ (up to Tahoe 26.1.x); 26.0 to 26.3 need Sequoia 15.6+; 26.4.1, 26.5, 26.6 need Tahoe 26.2+; Xcode 27 to 27.2 need Tahoe 26.6+. — [Apple Xcode system requirements](https://developer.apple.com/xcode/system-requirements)
- xcodes: prompts for Apple ID, or reads `XCODES_USERNAME` and `XCODES_PASSWORD`; stores the password in the keychain; needs superuser privileges to finish installation; uses aria2 for faster download if installed; the README does not document 2FA or headless operation. — [XcodesOrg/xcodes](https://github.com/XcodesOrg/xcodes)

### Inferences
- Apple ID with a developer-grade login is needed to download Xcode via xcodes (the xip comes from developer.apple.com); 2FA prompts require an interactive terminal (SSH session works) (inference from README gaps). An alternative is to download the .xip on the host and copy it into the guest.
- mist-cli and softwareupdate on Ventura may filter by host compatibility; mist-cli's "--compatible" flag being optional suggests incompatible versions are listable (inference, not verified).

### Gaps
- Not verified: whether softwareupdate on Ventura lists Sequoia/Tahoe installers.
- 2FA handling for xcodes in headless sessions not documented in the sources read.
- Apple developer download page requirements for Xcode (xip) not fetched.

## 6. Performance, sizing, and special setup

### Takeaway
No benchmark data found for Fusion or HVF compile performance on this CPU. The only evidence is qualitative: macOS guests have no Metal GPU acceleration, which is irrelevant for command-line builds; Sequoia guests on Intel under Fusion 13.6 have been reported to stall at boot.

### Cited Findings
- macOS VMs generally lack hardware Metal; CircleCI notes 3D acceleration for macOS guests "has never been greatly supported by the major hypervisors", and Metal-dependent tooling fails with "no available metal device". — [CircleCI support](https://support.circleci.com/en/articles/15091281-tests-fail-with-error-there-is-no-available-metal-device-on-this-system)
- Fusion 13.6 Sequoia guest on Mac mini 2018 (Sep 2024): boot often stalls at a coloured/black screen or kernel-panics; reducing cores or setting 1080p did not help; workaround edits `svga.present = "FALSE"` and `appleGPU0.present = "TRUE"` in the .vmx were only partly reliable; "Install VMware Tools" greyed out because Fusion 13.6 lacks darwin.iso, so copy darwin.iso from Fusion 13.5.2 and attach it as a SATA CD; one user said Sonoma and Sequoia fail only occasionally and Monterey is worse; repeated resets eventually boot. — [Broadcom community thread](https://community.broadcom.com/vmware-cloud-foundation/discussion/vmware-tools-for-macos-sequoia-150)
- Ventura-guest-on-Monterey-host boot loop reportedly tied to the removed Intel e1000 network driver; default NIC for macOS VMs may be e1000e. — [Broadcom community thread](https://community.broadcom.com/vmware-cloud-foundation/discussion/new-vm-macos-ventura-will-start-install-but-will-always-crash-when-booting-host-is-monterey-howto-solve)
- Fusion has no kernel extensions since 12 and uses Apple's Hypervisor framework (forum reply). — [VMware Communities](https://communities.vmware.com/t5/VMware-Fusion-Discussions/Fusion-12-1-1-crashes-Big-Sur-11-2-3/m-p/2843195)
- Disk sizes seen: installer images 15 to 16 GB (above); Apple's Xcode 26 downloads are on the order of many GB (no figure retrieved).

### Inferences (labelled as such, no sources)
- Sizing suggestion for an xnu build on 16 GB host: 6 vCPUs of 8 threads (leave 2 for the host), 8 GB guest RAM (leaves ~8 GB for host and Fusion), 100 GB or more virtual disk for macOS 15 + Xcode (~35 GB Xcode install plus caches) + xnu/SDK workspace; allocate at least 60 GB free beyond that. Use a pre-allocated or thin disk on the host SSD. Expect single-digit to roughly 20% CPU overhead for compile workloads under HV-based virtualization (typical, not measured here), but the 4-core i7-7820HQ is slow for large builds regardless; thermal throttling in a MacBook is likely.
- Time/clock: Fusion's VMware Tools time sync should be enabled or ntp used in the guest; not verified for macOS guests.
- Networking: NAT with port forward, or bridged, is expected to work for SSH; the Sequoia 15.4+ NAT DNS bug in Fusion 13.6.4 notes is for a Sequoia host, not this Ventura host.

### Gaps
- No compile-time benchmarks or overhead measurements for Fusion/HVF on Kaby Lake CPUs.
- No xnu-specific build size/time data retrieved (the 8 GB disk figure came from the task description).
- No source on Fusion's clock-drift behaviour in macOS guests.

## 7. Headless and automation: vmrun, VBoxManage, QEMU, SSH, snapshots, auto-start

### Takeaway
Fusion's vmrun supports `start <vmx> nogui`, snapshots (snapshot, listSnapshots, revertToSnapshot, deleteSnapshot) and getGuestIPAddress; guest-level commands need VMware Tools plus a guest login. Older reports say nogui needs a logged-in host user session and describe a launchd approach for boot start (versions 2 to 6; unverified for current Fusion).

### Cited Findings
- `vmrun -T fusion start "<path>.vmx" nogui` starts a headless VM; vmrun is hidden inside the Fusion app bundle. — [Art & Logic blog](https://artandlogic.com/?p=6267)
- Snapshot commands listSnapshots/snapshot/deleteSnapshot/revertToSnapshot and `getGuestIPAddress <vmx> [-wait]`; guest operations need VMware Tools and valid guest login. — [GitLab runner issue 1679](https://gitlab.com/gitlab-org/gitlab-ci-multi-runner/issues/1679); [vmrunpacked on PyPI](https://pypi.org/project/vmrunpacked/)
- A VMware Communities thread reports nogui refuses to start from CLI without a logged-in user; another notes vmware-vmx does the real work and the UI process can be killed after start; an old gist runs vmrun from a launchd plist at boot; a controlled guest shutdown over SSH is advised. — [VMware Communities](https://communities.vmware.com/thread/292873); [gist](https://gist.github.com/hedzr/f16f6f2b13a4a2c83d4ac92226749373)
- Fusion 25H2 adds a `dictTool` command-line utility to inspect and edit .vmx and preferences files. — [VMware blog](https://blogs.vmware.com/cloud-foundation/2025/10/14/vmware-workstation-fusion-25h2-embracing-calendar-versioning-and-new-features/)
- OSX-KVM provides a headless script but no out-of-box VNC/SSH. — [OSX-KVM README](https://github.com/kholia/OSX-KVM)

### Inferences
- SSH enablement inside the guest without a GUI is done during the one-time install (GUI via the Fusion window): enable Remote Login (System Settings, or `sudo systemsetup -setremotelogin on`, which needs Full Disk Access for the terminal on newer macOS) and enable auto-login so the guest reaches a stable session at boot. From then on the VM can run with `vmrun start nogui` and be reached over SSH.
- Snapshots: take a snapshot after the SSH-ready clean install and after Xcode installation, then revert as needed. Keep VMs powered off (not suspended) when upgrading Fusion/host.
- Auto-start at host boot needs a launchd LaunchAgent for the user (user session needed); no source found for current-Fusion behaviour.
- VBoxManage/QEMU headless options (`--type headless`, `-nographic`, `-vnc`) are only relevant if those hypervisors were used; no evidence they can host a Sonoma+ macOS guest on this host.

### Gaps
- No authoritative current Broadcom vmrun reference retrieved.
- Whether `vmrun nogui` still needs a logged-in host session on Fusion 13.6.x is unverified.
- No source for auto-start on the current Fusion.

## 8. Known failure reports for macOS 14/15/26 guests on Intel hosts

### Takeaway
Concrete reports exist for Fusion 13.6.x with Sequoia guests (boot stalls, kernel panics, no VMware Tools, install failures from the wrong image format) and for Parallels with macOS 26 guests (broken UI, crashes). I found no Tahoe-guest report for Fusion on Intel and no Sonoma-specific failure reports.

### Cited Findings
- Sequoia 15.0 guest on Intel Mac mini 2018, Fusion 13.6: black or coloured boot stalls, kernel panic, no darwin.iso, stuck at 1024x768 (details in section 6). — [Broadcom community](https://community.broadcom.com/vmware-cloud-foundation/discussion/vmware-tools-for-macos-sequoia-150)
- 2019 Intel MacBook Pro, Fusion 13.6.4, hosts Sonoma 14.7.6 then Sequoia 15.5: every guest install (High Sierra to Sequoia) ended at a black screen with a prohibitory sign until the installer was built via createinstallmedia on a plain disk image and the .dmg dragged into the setup window (resolved 29 Jul 2025). — [Broadcom community](https://community.broadcom.com/vmware-cloud-foundation/discussion/unable-to-install-macos-guest-on-2019-intel-macbook-reposting-as-discussion)
- Parallels macOS 26 guest on Intel: see section 2. — [Parallels forum](https://forum.parallels.cn/threads/macos-26-beta-intel-guest-on-macos-15-5-intel-host.367863/)
- Tahoe 26.5 host issue (May 2026): users reported Fusion VMs freezing after boot and new VMs not creatable; one tester had VMs powering on with 25H2u1; no fix recorded. Apple silicon hardware was mentioned; not a Ventura-host issue. — [Broadcom community](https://community.broadcom.com/vmware-cloud-foundation/discussion/macos-tahoe-265-and-fusion)
- Eclectic Light "Sequoia VMs can cause kernel panics" and the M4 virtualisation article concern Apple silicon hosts, not Intel. — [Eclectic Light](https://eclecticlight.co/2024/07/17/sequoia-vms-can-cause-kernel-panics/)
- A Broadcom thread titled "Fusion Causing Ventura Host Intel to Reboot" exists, but I could not open it, so the details are unknown. — (title only, in search results)

### Inferences
- Expect to need the createinstallmedia disk-image method, darwin.iso from an older Fusion for VMware Tools, and possibly several boot retries for Sequoia on Fusion 13.6.x. A Sonoma guest is probably the more stable choice for Xcode 16.2 (reported "fail only occasionally", as one user said), and Xcode 16.x on Sequoia 15.x also works per Apple's table.
- "No Metal" is not a blocker for xcodebuild/clang compile jobs, but simulators/UI tests would not work (inference).

### Gaps
- No Hacker News, MacRumors or Reddit posts retrieved on these topics (not reachable via the search results).
- No data on macOS 26 guest stability on Fusion 25H2 for Intel specifically.
- No reports for the specific host (MacBookPro14,3, 2017, Kaby Lake, Intel HD 630 plus Radeon Pro 555/560) were found.

## Summary of options for this host (all inferences from the above)
1. Best fit: VMware Fusion 13.6.4 (free, Broadcom login) with a Sonoma 14.x guest (Xcode 16.2) or Sequoia 15.6+ guest (Xcode 16.2 to 26.3). Official: Sequoia guest on Intel added in 13.6; Ventura as host supported.
2. Xcode 26.4 and later (Tahoe 26.2+ required) is not reachable on this host, unless an unofficial Tahoe guest boots on Fusion 13.6.x (no evidence either way), or the host is changed (e.g. Fusion 25H2 needs a host of macOS 15+ per a mirror).
3. Parallels 26 (paid, Intel guests listed to Sequoia 15.0) and QEMU/UTM/VirtualBox/Virtualization.framework add no demonstrated benefit; Virtualization.framework and UTM macOS guests are Apple silicon only.
