# Docker and Docker-like runtimes as a way to build xnu with a newer macOS/Xcode/SDK than the host supports

Context read from the repo (verified by reading the files): `.github/workflows/xnu.yml` builds xnu through blacktop/darwin-xnu-build (`build.sh`, pinned by `DXB_SHA`) on GitHub macOS runners: macos-15 with Xcode 16.2 for xnu-11215 (15.x), macos-26 with Xcode 26.1 for xnu-12377 (26.x), macos-14 with Xcode 15.x for 13.x/14.x. It needs Apple's own host tools (Xcode's `iig`, `xcrun`, `ctfconvert`/`ctfmerge` built from Apple's dtrace repo, `mig` from bootstrap_cmds), the macOS SDK with Kernel.framework private headers, and optionally the KDK (macOS-only installer). `tools/xnu-local-build.sh` needs "about 8 GB free", uses sccache/ccache, `-j8`, and requires "Xcode 26.x (DEVELOPER_DIR=...)" (Xcode 27 SDK drops the private headers). `docs/CI_CACHE.md` records CI Build step times of roughly 3-8 minutes on macos-14 runners (cold 5:42-8:31, warm 3:22-4:43), plus ~1 min KDK install and ~75 s `xnu_headers`. So the build is a real macOS-toolchain build, not a plain clang compile.

## 1. "macOS in Docker" (Docker-OSX, dockur/macos): how it works, versions, KVM requirement, what happens without it

### Takeaway
"macOS in Docker" means a QEMU virtual machine running a real macOS guest inside a Linux container; it is not a macOS container. It hard-requires `/dev/kvm` on a Linux (or Windows 11 + WSL2 nested-virt) host, and the projects themselves say Docker Desktop on macOS is unsupported because it provides no KVM. On the user's Ventura Intel Mac it will not run in any useful form.

### Cited Findings
- Docker-OSX supports guest images from High Sierra (10.13) through Tahoe (16 in its naming); Quick Start covers Sequoia and Tahoe via `-e SHORTNAME=sequoia|tahoe` on `sickcodes/docker-osx:latest`; its technical-details list stops at Sonoma. Requirement is "a x86_64 kvm-capable host", every run command passes `--device /dev/kvm`; documented hosts are Linux and Windows 11 (build 22000+) via WSL2 with `nestedVirtualization=true`. The README does not mention macOS hosts. Defaults: 3 GB RAM, 20 GB+ disk (50 GB with Xcode; `:auto` image at least 50 GB). It advertises "near-native performance" (with KVM) — [Docker-OSX README](https://github.com/sickcodes/Docker-OSX)
- Docker-OSX is GPL v3+; macOS itself is not distributed in the image, it is downloaded from Apple during setup (README is silent on this; see dockur below for an explicit statement) — [Docker-OSX README](https://github.com/sickcodes/Docker-OSX)
- Docker-OSX FAQ, "Can I run on macOS?": for Apple Silicon Macs it recommends UTM instead; for Intel Macs "Docker runs inside a Linux VM, which causes common errors" and it suggests running QEMU directly with HVF acceleration (e.g. through libvirt) — [Docker-OSX FAQ](https://github.com/sickcodes/Docker-OSX/blob/master/FAQ.md)
- dockur/macos (the other widely used macOS-in-Docker project): `VERSION` accepts 15 (Sequoia, default), 14, 13, 12, 11; macOS 26 (Tahoe) is selectable but "not recommended yet" because it runs very slowly. KVM is required and an AVX2-capable CPU. Supported hosts: Docker/Podman on Linux with KVM, Docker/Podman Desktop on Windows 11 with nested virtualization. "Docker Desktop on Linux, macOS, and Windows 10 is not supported because it does not provide KVM access to containers." Defaults 2 cores, 4 GB RAM, 64 GB disk; minimum 4 GB RAM and 32 GB free disk — [dockur/macos](https://github.com/dockur/macos)
- Docker-OSX issue tracker has no answered issue about running on a macOS host without KVM; open issues are about Apple Silicon (#547 "How to get docker-osx running on mac silicon", #459 native Apple Silicon support) — [Docker-OSX issues](https://github.com/sickcodes/Docker-OSX/issues?q=macos+host+kvm)
- Without KVM, QEMU falls back to TCG software emulation. One 2026 guide (for Windows guests, not macOS) says a boot goes "from 40 seconds to 25 minutes" without KVM; a search-result summary of that guide, not the full article — [pistack.xyz, Windows and macOS in Docker in 2026](https://www.pistack.xyz/posts/2026-10-18-windows-macos-in-docker-dockur-docker-osx-qemu/)
- Docker Desktop does not document KVM/nested virtualization for Mac containers anywhere I could find; third-party READMEs (dockur) state the limitation. On Apple Silicon, Lima's nested virtualization (VZ driver) is limited to M3+ devices on macOS 15+; I found nothing for Intel Macs — [Lima PR #2530](https://github.com/lima-vm/lima/pull/2530)
- Parallels documents nested virtualization on Intel Macs only when the host VM uses the Parallels hypervisor (a paid product, a different path than Docker) — [Parallels developer guide](https://docs.parallels.com/landing/parallels-desktop-developers-guide/software-development-specific-functions-of-parallels-desktop/nested-virtualization-support)
- VMware Fusion Intel nested virtualization is flaky: community reports of "VMware Fusion does not support nested virtualization on this host" / HV module power-on failures when "Enable hypervisor applications" is ticked, and MSR write errors when running QEMU in a Linux guest on an Intel Mac host (anecdotal, Fusion 12-era) — [VMware Communities](https://communities.vmware.com/t5/VMware-Fusion-Discussions/Attempting-to-do-nested-virtualization/m-p/2897723)

### Inferences
- A chain "Ventura host -> Linux VM (needs nested VT-x) -> Docker -> QEMU/KVM -> macOS Sequoia/Tahoe guest" stacks three virtualization layers on a 4C/8T, 16 GB machine; if nested VT-x is unavailable it degrades to TCG, which is not practical for macOS (and dockur says even KVM-accelerated Tahoe is very slow). The container layer adds nothing: the thing that does the work is QEMU, which can be run directly on the Mac with HVF (as the Docker-OSX FAQ itself recommends for Intel Macs).
- The practical conclusion from the FAQ is that the container is pointless on a Mac host: skip Docker and run a macOS VM directly (section 6).
- TCG-emulated macOS for an xnu build (hundreds of compile jobs, Xcode 16.2/26.1 toolchains, 8 GB+ working tree) would be orders of magnitude slower than the 3-8 minute CI builds; I would treat it as infeasible rather than "slow".

### Gaps
- No measured TCG speed for macOS guests; no Docker-OSX report of success on an Intel Mac host; no official Docker statement on KVM in Docker Desktop for Mac. I did not run anything on the user's machine.
- Whether Docker Desktop's Docker VMM (Beta, per docs) or the Apple Virtualization framework backend exposes nested virtualization on Intel: not documented in what I found.

## 2. Docker on a macOS host: Linux containers only, Docker Desktop and alternatives on Ventura/Intel in 2026

### Takeaway
Containers on a Mac are always Linux (in a hidden VM); there are no macOS container images. Current Docker Desktop (4.97.0, 2026-10-26) still ships an Intel build but needs macOS 14+, so a Ventura host is stuck on an old release (4.48.0 per secondary sources) or must use Colima/Lima/Podman; OrbStack also needs macOS 14+.

### Cited Findings
- Docker Desktop release notes: latest is 4.97.0 dated 2026-10-26; the download list still includes "Mac (Intel chip)" — [Docker Desktop release notes](https://docs.docker.com/desktop/release-notes/)
- Docker's install page: Docker Desktop "is supported on the current and two previous major macOS releases", with separate Intel and Apple silicon downloads; need at least 4 GB RAM. With macOS 26 current (Nov 2026) that means macOS 26, 15, 14 — [Docker Desktop Mac install](https://docs.docker.com/desktop/setup/install/mac-install/)
- Docker Desktop 4.48.0 (October 9, 2025): "Important: Support for macOS 13 has ended. Installing Docker Desktop will require macOS 14 in the next release." — [releases.sh mirror of the release note](https://releases.sh/release/rel_4h-qp4sxELNzCRe9NkFut-macos-13-support-ending-pac-proxy-configuration-added). Secondary summaries: 4.48.0 is the last for Ventura 13.3+, 4.41.2 for 13.0-13.2; Shipyard says Ventura users "will need to find an alternate solution" — [web search summary](https://github.com/docker/docs/pull/23503/files), [Shipyard](https://shipyard.build/blog/installing-docker-on-older-mac/)
- Docker Desktop for Mac settings: VMM choices are Apple Virtualization framework or Docker VMM (Beta); file sharing default VirtioFS (gRPC FUSE alternative with the Virtualization framework VMM; Docker VMM supports only VirtioFS); memory limit defaults to 50% of host memory, swap 1 GB — [Docker Desktop settings](https://docs.docker.com/desktop/settings-and-maintenance/settings/)
- OrbStack is not Apple-Silicon-only: its download page lists an Intel build, but states "Supports macOS 14.0+", so it does not run on Ventura — [OrbStack download](https://orbstack.dev/download)
- Lima's QEMU backend uses QEMU with HVF on macOS; Colima uses the latest Lima from Homebrew and inherits its requirements — [Rancher Slack archive](https://slack-archive.rancher.com/t/26840557/hey-all-wave-i-have-a-question-about-rancher-desktop-and-nes), [Lima vmType docs](https://lima-vm.io/_print/docs/config/vmtype/)
- Windows containers are irrelevant on a Mac host (no source needed; Docker Desktop for Mac runs only a Linux VM; the dockur page above lists macOS as unsupported for KVM-based images).

### Inferences
- On Ventura the usable options are: old Docker Desktop 4.48.0, or Colima/Lima (QEMU+HVF or VZ), or Podman machine. Their Homebrew/bottle support for Ventura (macOS 13 is Tier 3/unsupported by Homebrew in 2026 — unverified) may be a further obstacle; I did not verify Colima/Podman/Rancher Desktop minimum macOS versions.
- Any of these gives only a Linux kernel; none can run Xcode, `iig`, `xcrun`, KDK.

### Gaps
- Primary-source confirmation of the 4.48.0 cut-off (I saw the release note via a mirror and a French guide, not the docs.docker.com archive page, which exceeded the fetch window).
- Minimum macOS for Colima, Lima, Podman Desktop, Rancher Desktop in Nov 2026 not verified.

## 3. Docker as a Linux cross-compile environment for macOS (osxcross etc.): what the container adds, 16 GB limits, bind-mount performance, can it build xnu

### Takeaway
A container only packages a Linux userland; it is the same as a plain Linux VM plus reproducibility. On a 16 GB Mac it costs RAM and I/O overhead on top, and it does not remove the real blocker for xnu, which is Apple-only tooling.

### Cited Findings
- osxcross: SDK must be extracted by the user "from the full Xcode or from the Xcode Command Line Tools" and put in `tarballs/`; host systems are Linux and *BSD; targets arm64, arm64e, x86_64, i386; flavors use cctools 986/ld64 711 (stable), cctools 1030.6.3/ld64 956.6 (latest) or LLVM `ld64.lld`. Docker is mentioned only as the multiarch/crossbuild project using it — [osxcross README](https://github.com/tpoechtrager/osxcross)
- Docker Desktop for Mac: VirtioFS default, claimed up to 98% reduction in filesystem operation time vs older sharing; 50% of host RAM by default (8 GB on this 16 GB machine) — [Docker Desktop settings](https://docs.docker.com/desktop/settings-and-maintenance/settings/)
- The xnu workflow depends on macOS-only pieces: `xcrun --find iig`, Xcode's SDK incl. Kernel.framework PrivateHeaders, host `mig`/`ctfconvert` built with Xcode, KDK installer — read from `/Users/luigicolucci/git/Bergamota/.github/workflows/xnu.yml` and `tools/xnu-local-build.sh`.
- Licensing: Apple Developer Forums answer says the Xcode licence's Copies section "does not allow for using any part of XCode on anything but Apple-branded hardware" (restriction present since at least 2011); a Python bug-tracker commenter (non-lawyer) reads the grant as tied to hardware, not OS; Baeldung says running the Xcode app on Linux violates the EULA. Unresolved; not legal advice — [Apple forums](https://developer.apple.com/forums/thread/114179), [python bug](https://bugs.python.org/msg173504)

### Inferences
- Bind mounts of a large source tree through a VM boundary are slower than native APFS even with VirtioFS; keeping the tree and ccache inside the container/VM filesystem avoids that, but then it takes more of the 246 GB disk and RAM split (container VM + host).
- xnu needs `iig`, `mig` from bootstrap_cmds built with Xcode clang, `ctfconvert` and the private SDK headers; osxcross provides clang + cctools linker + SDK but not iig or Apple's codesign/dtrace tools. This is the territory of the other researcher (osxcross/Darling); my view is that Docker neither helps nor hurts feasibility, it only packages whatever that approach yields. And a Linux container extracting the SDK means the SDK itself must be pulled from an Xcode 16.2/26.1 xip, which only unpacks on macOS or via third-party tools (unverified here).
- Even if technically possible, using Xcode-derived SDKs on Linux sits in a licence grey area (above); on Apple hardware inside a macOS VM the situation is cleaner.

### Gaps
- No source confirming anyone has built xnu (xnu-11215/12377) with osxcross in a container. No measured VirtioFS vs native numbers for a tree the size of xnu.

## 4. Self-hosted GitHub Actions runner or `act` locally: can it run macOS jobs?

### Takeaway
`act` cannot run `runs-on: macos-*` jobs on a Docker host as macOS; it runs jobs in Linux containers using Docker images. A self-hosted GitHub Actions runner can run macOS jobs only if it is installed on an actual macOS machine (or macOS VM), which is a way to reuse the repo's workflow unchanged on whatever macOS VM/remote Mac you have.

### Cited Findings
- act "uses the Docker API to run containers for each action"; the README does not mention macOS labels — [nektos/act](https://github.com/nektos/act). I did not find a primary statement that macOS labels are unsupported; the evidence is that a fork (act-cli by actions-oss/ChristopherHX) advertises added "macOS VMs using tart" support, implying upstream lacks it — [gitea.com/actions-oss/act-cli](https://gitea.com/actions-oss/act-cli/src/)
- GitHub's Actions runner supports macOS (High Sierra+) as a self-hosted runner OS — [actions/runner docs mirror](https://github.com/ajaykn/runner/blob/main/docs/start/envosx.md)
- The existing workflow already has a `runner` input to override the runner label (`runs-on: ${{ inputs.runner || ... }}`) — read from `xnu.yml`, so it can target a `self-hosted` label with no workflow change.

### Inferences
- act is useful only for the non-Apple parts (linting scripts, Linux jobs); it cannot validate the xnu job.
- The runner label override makes a self-hosted macOS VM or a rented Mac a drop-in way to run this same job.

### Gaps
- No official nektos/act text on macOS labels; confirmation is by absence plus the fork's feature claim.

## 5. Apple EULA / licensing for macOS in a Docker/QEMU container

### Takeaway
Apple's macOS licence permits virtual macOS instances only on Apple-branded hardware; Docker-OSX and dockur both say so themselves. The user's MacBookPro14,3 is Apple-branded, so a macOS VM there is within the licence, subject to the terms below; a Docker-OSX container on non-Apple (Linux/cloud/Windows) hardware is not.

### Cited Findings
- macOS Tahoe 26 SLA ("For use on Apple-branded Systems"), 2.B(iii): licensee may "install, use and run up to two (2) additional copies or instances of the Apple Software, or any prior macOS or OS X operating system software or subsequent release of the Apple Software, within virtual operating system environments on each Apple-branded computer you own or control that is already running the Apple Software, for purposes of: (a) software development; (b) testing during software development; (c) using macOS Server; or (d) personal, non-commercial use." Not permitted for service bureau, time-sharing etc. (except per Section 3). 2.A: single-copy licence for "one (1) copy of the Apple Software on a single Apple-branded computer at any one time" — [Apple macOS Tahoe SLA PDF](https://www.apple.com/legal/sla/docs/macOSTahoe.pdf) (text extracted locally with PDFKit)
- Section 3 (leasing) allows a Lessor/Lessee to virtualize per 2B(iii), limited to a single instance as provisioning tool for lessors (same SLA).
- Docker-OSX FAQ: "The macOS software license allows running (some versions of) macOS in a virtual machine only on Apple hardware"; Apple Security Bounty terms make an exception for security research; other uses violate the licence; not legal advice — [Docker-OSX FAQ](https://github.com/sickcodes/Docker-OSX/blob/master/FAQ.md). README: trademarks "do not sponsor or endorse this project"; links to a legal FAQ — [README](https://github.com/sickcodes/Docker-OSX)
- dockur/macos disclaimer: "Only run this container on Apple hardware, any other use is not permitted by their EULA"; installing macOS requires accepting Apple's EULA; project ships no macOS — [dockur/macos](https://github.com/dockur/macos)
- Xcode is covered by its own licence; see section 3 on the Apple-branded-hardware restriction.
- AWS attributes its 24-hour minimum Mac host allocation to the Apple macOS SLA — [AWS EC2 Mac](https://aws.amazon.com/ec2/instance-types/mac/)

### Inferences
- A macOS Sequoia/Tahoe VM on the user's own MacBookPro14,3 for software development/testing/personal use falls inside 2.B(iii) as quoted (host is Apple-branded and already running macOS; the clause explicitly includes "subsequent release", so a newer guest than the host is covered; two extra instances max). I read the Tahoe SLA; the licence shipped with the Sequoia/Sonoma installer may differ in wording — not checked. Not legal advice.
- Docker-OSX on the user's Apple hardware would be EULA-compatible in principle (Apple hardware, VM), but it cannot work technically there (no KVM), so the point is moot. Running it in a cloud Linux VM/CI is not compatible.
- GitHub-hosted macOS runners and AWS EC2 Mac are Apple hardware, which is why they are licence-clean.

### Gaps
- Licence text of the Xcode 16.2/26.1 SLA not read directly; relies on forum quotes.
- No legal advice sought; EU jurisdictional supplement not analysed.

## 6. Practical comparison for this user: Docker-OSX vs VMware Fusion / QEMU+HVF VM vs remote Apple Silicon Mac

### Takeaway
Recommendation: do not use Docker. If a local solution is wanted, run a macOS 15 (Sequoia) VM directly on the Ventura Intel host with VMware Fusion 13.6.x (Sequoia guest officially listed for Intel; enough for Xcode 16.2 / xnu-11215). For xnu-12377 / Xcode 26.1+ the simplest reliable choice is the CI macOS runners you already use or a rented Apple Silicon Mac. Tahoe guest on this Intel host is unproven.

### Cited Findings
- Xcode requirements (Apple): Xcode 16.2 needs macOS Sonoma 14.5 to Sequoia 15.x; 16.4 needs 15.3 to 26.1.x; Xcode 26.0-26.3 need Sequoia 15.6 to Tahoe 26.x; 26.4.1 and 26.5 need Tahoe 26.2+. The page does not mention Intel Macs — [Apple Xcode system requirements](https://developer.apple.com/xcode/system-requirements/)
- macOS Tahoe 26 supports only four Intel Macs: MacBook Pro 16-inch 2019, MacBook Pro 13-inch 2020 (four Thunderbolt 3 ports), iMac 5K 2020, Mac Pro 2019. The 2017 MBP is not among them — [Apple support, macOS Tahoe compatibility](https://support.apple.com/en-us/122867)
- VMware Fusion 13.6 release notes list "New Guest Operating System Support On Intel Chipset: macOS Sequoia"; 13.6.1 notes list hosts macOS 13, 14, 15 (Intel or Apple silicon); no official Tahoe guest or host statement found; a community guide covers booting unsupported macOS guests in Fusion on Intel Macs — [Fusion 13.6 notes](https://docs.vmware.com/en/VMware-Fusion/13.6/rn/vmware-fusion-136-release-notes/index.html), [Fusion 13.6.1 notes](https://docs.vmware.com/en/VMware-Fusion/13.6.1/rn/vmware-fusion-1361-release-notes/index.html), [tinyapps.org](https://tinyapps.org/blog/run-unsupported-macos-vm.html), [Broadcom community summary](https://community.broadcom.com/vmware-cloud-foundation/discussion/path-forward-from-ventura-1376f)
- QEMU's x86 HVF backend can accelerate same-arch guests on Intel Macs; the 2023 patch series claimed only "a few percent" gains and ran macOS guests in CI; a Haswell MacBook on macOS 10.15 could only boot a (non-macOS) guest under TCG in one forum report — [QEMU patchew](https://patchew.org/QEMU/20230922140914.13906-1-phil@philjordan.eu/), [Haiku forum](https://discuss.haiku-os.org/t/qemu-on-macos-host-rather-slow-performance-in-haiku-guest/10687)
- AWS EC2 Mac: Dedicated Hosts, Apple silicon (M1 to M4 Max/M3 Ultra) and one Intel type (`mac1`, i7 6-core 32 GB), per-second billing with a 24-hour minimum allocation tied to the SLA; M1 is advertised as "up to 60% better price performance" than x86 for build workloads — [AWS EC2 Mac](https://aws.amazon.com/ec2/instance-types/mac/)
- This repo's own CI already runs the job on GitHub macos-14/15/26 hosted runners, 3-8 min Build step (`docs/CI_CACHE.md`); a recent commit notes the macos-15 arm64 pool is often full and added a runner override (`e2e8f6a`).

### Cited comparison (my assessment; ratings are inference)

| Option | Speed | Setup effort | Risk |
|---|---|---|---|
| Docker-OSX / dockur on this Mac | TCG-only or impossible (no KVM) | High, then fails | Wasted time; Docker Desktop no longer supports Ventura |
| Fusion 13.6 (or QEMU+HVF) macOS 15 guest, Xcode 16.2 | Probably close to native CPU for compile; I/O and RAM-limited (give ~8-10 of 16 GB; 4-6 vCPUs of 8 threads) | Medium: fetch the Sequoia installer, create VM, install Xcode 16.2 (~50 GB disk) | Medium: Sequoia on an unsupported Mac model/Intel Fusion quirks; Xcode 26.x/Tahoe not covered |
| Same VM for xnu-12377 (Xcode 26.1 needs 15.6+) | Same | Same plus Xcode 26.1 on Sequoia 15.6+ (listed as supported by Apple's table) | Medium-high; KDK/26 SDK items per `xnu-local-build.sh` |
| Remote Apple Silicon (GitHub runner, self-hosted label, or rented/AWS Mac) | Fastest (the existing 3-8 min CI builds) | Low: already in place; or EC2 Mac setup | Capacity of macos-15 pool; AWS 24 h minimum; cost |

### Inferences
- Because Xcode 26.1 only needs Sequoia 15.6+, a Sequoia 15.6 VM (not Tahoe) is enough for xnu-12377 per Apple's table, so the unproven Tahoe-on-Intel-Fusion question may not matter. But `xnu-local-build.sh` comments say Xcode 26.x is expected and that macOS 26.x KDKs are used when `USE_KDK=1`; the KDK for 26.5 may demand a Tahoe host (unverified).
- Fusion for macOS guests on an Intel host plus 16 GB RAM leaves little headroom: guest 8 GB + host 8 GB; the xnu build fit "8 GB free disk" per the script, but concurrent clang jobs at `-j8` may swap. Lowering `-j` is likely needed.
- Setup effort for the VM is mostly obtaining a Sequoia installer: on Ventura hardware that the Mac App Store/softwareupdate may not offer Sequoia for an unsupported model; `softwareupdate --fetch-full-installer` or `mist-cli` are possible routes (not verified here).
- Cheapest first step: keep CI as the build farm (it is already working), use `gh workflow run` with the `runner` input, and only invest in a local VM if CI capacity/queue becomes the bottleneck.

### Gaps
- No benchmark of an xnu build in a Fusion/QEMU macOS VM on Intel i7-7820HQ; all speed claims are estimates.
- Whether Fusion 13.6.x Sequoia guest actually installs/boots on a Kaby Lake Intel host under Ventura is supported by release notes only, not by a first-hand report found; Tahoe guest on Intel Fusion unconfirmed.
- Price of rented Apple Silicon Macs (MacStadium, AWS rates) not found; AWS page lists no hourly rate.
