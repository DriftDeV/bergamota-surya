# Remote and dedicated macOS build environments (non-GitHub-hosted) for Xcode 16.2 / Xcode 26.x

Research date: 2026-11-02. Prices are in the currency the source uses (EUR where the source gave EUR; USD otherwise). Conversions are my arithmetic and are flagged. Verified facts are in "Cited Findings"; everything else is in "Inferences" or "Gaps".

## 1. Used / refurbished Apple Silicon Mac as an always-on headless build box

### Takeaway
A refurbished Mac mini M1 (about EUR 500-700) or M2 (about EUR 380-700) runs macOS 26 and is roughly 2-2.5x faster than a 2017 Intel i7 MacBook Pro on the XcodeBenchmark test. A refurbished M4 16 GB costs about EUR 600-880. Prices are aggregator snapshots, not live Italian quotes. The headless setup works (SSH, Screen Sharing, auto-login) but has one known keychain gotcha when running VMs.

### Cited Findings
- M1 Mac mini (2020) is listed as supported by macOS Tahoe 26 in several compatibility lists — [9to5Mac](https://9to5mac.com/2025/06/09/macos-tahoe-26-compatibility-list/), [Neowin](https://www.neowin.net/news/here-are-the-macs-that-support-macos-26-tahoe/) (summarised by search tool; I did not read Apple's official list).
- Mac mini M1 (2020), eBay Refurbished, 7 Oct 2026: EUR 499 (8 GB / 256 GB). Mac mini M4 16 GB / 256 GB on eBay Refurbished the same day: EUR 883 — [RefurbMe stats, Mac mini M4](https://www.refurb.me/it/stats/mac-mini/mac-mini-m4-fine-2024).
- Mac mini M4 (16 GB / 256 GB) refurbished by Apple Store: EUR 829 (undated). Lowest recorded refurbished price for the same configuration: EUR 596 (2026-05-18). New: EUR 899 on an Amazon Italy deal vs EUR 959 list (snapshot, unverified date) — [RefurbMe](https://www.refurb.me/it/stats/mac-mini/mac-mini-m4-fine-2024), [Punto Informatico](https://www.punto-informatico.it/risparmia-mac-mini-apple-m4-sconto-record/).
- Mac mini M2 (2023): reBuy/Certideal EUR 594 and 691 (excellent condition; 256 GB with 8 or 16 GB), EUR 379 for 8 GB in "good" condition (Certideal), "from" price EUR 551 — [RefurbMe](https://www.refurb.me/it/refurbished/mac-mini/with-apple-silicon).
- Back Market elsewhere in Europe: M1 2020 from EUR 700.95 (Ireland), EUR 633 for 8 GB / 512 GB in "Bon" and EUR 834 for 16 GB / 1 TB (France), from EUR 598 (Germany). I found no live Back Market Italy listing — [Back Market IE](https://www.backmarket.com/en-us/l/mac-mini-late-2020-m1/22a20b17-3bc7-4d3f-af55-f35666a882c0), [refurb.me FR](https://www.refurb.me/fr/refurbished/mac-mini/by-backmarket), [refurb.me DE](https://www.refurb.me/de/refurbished/mac-mini/by-backmarket). The German page also lists a "Mac mini M6, Ende 2026", which I could not confirm exists; treat that aggregator's data with caution.
- XcodeBenchmark (large framework with 76 CocoaPods libraries; time in seconds, lower is better). The project says Xcode 15-and-earlier results are not comparable to newer ones — [XcodeBenchmark](https://github.com/devMEremenko/XcodeBenchmark):
  - Xcode 15.x: Mac mini M1 16 GB (Xcode 15.2, macOS 13.6.3) 175 s; MacBook Pro 15" 2018 i7 6-core (Xcode 15.0, macOS 14) 348 s.
  - Xcode 12.2 / 11.0.1: MacBook Pro 15" 2017 i7 4-core 282 s; Mac mini M1 (Xcode 12.2) 116 s.
  - Xcode 16.1: Mac mini M4 10-core 16 GB 141 s; M4 Pro 12-core 103-109 s; M4 Pro 14-core 96 s.
  - Xcode 26.x/27: M4 Mac mini 185 s (Xcode 27.0, macOS 27.0); M4 Pro 14-core 146 s (Xcode 26.4, macOS 26.3.1).
  - M2 Mac mini 109-111 s (Xcode 14.2).
- Headless caveat from the Tart FAQ: since macOS 15, Virtualization.framework needs an unlocked login keychain for a VM to start; the fix is to log in once via Screen Sharing and enable automatic login, or unlock the keychain from Terminal (`security unlock-keychain`); keychain errors can also appear in SSH sessions — [Tart FAQ](https://tart.run/faq/).
- The macOS-26 Xcode benchmark entries show Xcode 26 compiles are slower than Xcode 16.1 on the same M4 Pro (146 s vs 96-109 s), with the caveat that benchmark versions differ — [XcodeBenchmark](https://github.com/devMEremenko/XcodeBenchmark).

### Inferences
- For xnu (C/C++ with clang, high parallelism) an M1 should land around 2-2.5x faster than the 2017 4-core i7 if the XcodeBenchmark ratio (116 vs 282 s on the same Xcode 12.2) carries over. This is only an analogy; no xnu-specific benchmark was found.
- The user's 15-minute GitHub arm64 figure (see the workflow) is probably similar to an M1 mini; a 10-core M4 should be faster per core. Not measured.
- Choose 16 GB RAM and 512 GB SSD: Xcode (two versions) plus 25 GB Tart base images do not fit comfortably on 256 GB. An 8 GB M1 for EUR 499 is a poor fit if VMs are used.
- From background knowledge (not sourced here): Xcode 16.2 needs macOS 14.5+ and Xcode 26 needs macOS 15.6+ or later, so an M1 on macOS 26 runs both. Whether macOS 27 still supports M1 was not verified.
- A mini at idle draws only a few watts (background knowledge; not sourced), so running it 24/7 is cheap. The break-even versus renting (below) is roughly 3-5 months of a EUR 149/month rental for an EUR 500-600 box.

### Gaps
- No live Subito.it / Back Market Italy / Apple Refurbished Italy prices were accessible; figures are aggregator snapshots from May-October 2026.
- No xnu-specific compile time on M1/M2/M4 bare metal.
- Not verified: whether macOS 27 drops M1.

## 2. Rented Macs (AWS EC2 Mac, Scaleway, MacStadium, MacinCloud, Orka, others)

### Takeaway
Rentals fall into two models: per-hour with a 24 h minimum (AWS, apparently Scaleway) at roughly USD 15-30 (AWS M1-M4) or about EUR 5 (Scaleway M4-S, per a competitor page) per use, and flat monthly (MacStadium, MacinCloud, Scaleway) at USD 109-349/month. Occasional builds fit AWS/Scaleway hourly; steady use fits a purchased mini sooner.

### Cited Findings
- AWS EC2 Mac is sold as Dedicated Hosts. Billing is per second with a 24-hour minimum allocation (to comply with Apple's macOS license); after that you can release at any time. Types: mac2 (M1, 16 GB, Mac mini, macOS Big Sur and later), mac2-m2, mac2-m2pro, mac-m4 (24 GB), mac-m4pro (48 GB), mac-m4max, mac-m3ultra, mac1 (Intel i7). Savings Plans up to 44% off with 3-year commitment — [AWS EC2 Mac](https://aws.amazon.com/ec2/instance-types/mac/).
- AWS On-Demand Dedicated Host prices, us-east-1, retrieved 14 Jul 2026 (USD/h): mac2 M1 0.65; mac2-m2 0.878; mac2-m2pro 1.56; mac-m4 1.23; mac-m4pro 1.97; mac-m4max 6.25; mac1 Intel 1.083 — [Classmethod via AWS Pricing API](https://dev.classmethod.jp/en/articles/ec2-mac-m3ultra-metal-pricing-api/).
- Scaleway (Apple silicon page, undated): Mac mini M4-S 16 GB / 256 GB EUR 0.22/h or EUR 149/month; M4-M 32 GB / 1 TB EUR 0.29/h or EUR 199/month; M4 Pro M4-XL 64 GB / 2 TB EUR 0.49/h. A competitor comparison page says Scaleway bills M4 hourly with a 24 h minimum (about EUR 149-160/month if run continuously); I did not find the minimum on Scaleway's own pages — [Scaleway pricing (via search)](https://scaleway.com/en/pricing/apple-silicon), [Green Mini comparison](https://www.greenmini.nl/compare/scaleway-alternative/). The Scaleway pages themselves returned HTTP 403 to my fetches.
- MacStadium monthly Mac mini pricing (USD, billed monthly in advance, annual/multi-year prepay available; no hourly option listed): M4.S (M4, 16 GB, 256 GB) 149; M4.M (24 GB, 512 GB) 249; M4.L (M4 Pro 48 GB, 1 TB) 349; M2.S (8 GB) 109; M2.M (16 GB, 1 TB) 199; M2.L (M2 Pro, 16 GB) 249; M2.XL (M2 Pro, 32 GB, 2 TB) 349. Orka prices are not published (contact sales; also on AWS Marketplace) — [MacStadium pricing](https://www.macstadium.com/pricing).
- MacinCloud (via competitor page dated June 2026): dedicated M4 USD 124.99/month or USD 1,249/year; M2 USD 99/month or 490/year; managed shared plans about USD 25-29/month; pay-as-you-go about USD 1/hour. G2 reviewers mention no root access and that pay-as-you-go billing was "not great" — [Green Mini comparison](https://www.greenmini.nl/compare/macincloud-alternative/), [G2 review](https://www.g2.com/products/macincloud/reviews/macincloud-review-4627010).
- Other rental sites surfaced but not evaluated: Macweb, Neolo, rentamac.io, vboxx.eu (e.g. Macweb M4 Max USD 599/month) — [whtop listing](https://www.whtop.com/plans/macweb.com/144047).

### Inferences
- Cost of one minimum AWS allocation (my arithmetic, USD, 24 h x hourly rate): M1 about 15.6; M2 about 21.1; M4 about 29.5; M4 Pro about 47.3. A single 15-minute build is therefore uneconomic on AWS unless you batch many builds into the 24 h (e.g. build both Xcode versions, iterate on patches).
- Scaleway M4-S 24 h minimum would be about EUR 5.3 per allocation (0.22 x 24; depends on the unverified minimum) — the cheapest rental route for occasional use, and EU-based. Whether its image offers macOS 15/26 and Xcode was not verified.
- AWS and Scaleway both give SSH-capable remote hosts, so they fit "headless over SSH". Setup effort is higher on AWS (account, Dedicated Host quota, AMI, EBS, release discipline) than Scaleway.
- Past roughly 1 month of continuous use, a purchased used M1/M2 mini is cheaper than any monthly rental listed.

### Gaps
- Hetzner: I did not research or find evidence of a Mac offering; not verified either way.
- Orka and Mac mini colocation pricing: no public figures found.
- Scaleway minimum billing period and available macOS/Xcode versions: not confirmed from Scaleway.
- Current AWS EC2 Mac region availability in Europe (Frankfurt/Milan/Ireland) and per-region price: not checked.

## 3. Other CI providers (macOS arm64) usable with a public GitHub repo

### Takeaway
Cirrus CI, the obvious cheap option, shut down on 1 June 2026 and Cirrus Runners no longer takes new customers. Free tiers elsewhere are small: CircleCI about 150 M4 Pro minutes/month, Codemagic 500 M2 minutes/month (personal accounts), Bitrise about 150 macOS minutes/month. At about 15 minutes per build that is roughly 10-30 builds a month. GitLab macOS runners need Premium or an OSS program. Xcode Cloud is not a generic shell runner.

### Cited Findings
- Cirrus Labs announced on 7 April 2026 that it is joining OpenAI; Cirrus CI shut down on Monday 1 June 2026; Cirrus Runners (USD 150 per runner per month, unlimited minutes) stopped accepting new customers and existing contracts are supported through their term — [Cirrus Labs announcement](https://cirruslabs.org/), [paddo.dev](https://paddo.dev/blog/ushr-launch/).
- CircleCI: macOS executor available on the Free plan; from 10 Nov 2025 the default macOS class for Free is m4pro.medium; free organisations get 30,000 free credits per month for macOS open-source builds — [CircleCI changelog](https://circleci.com/changelog/free-plan-default-macos-resource-class-changes-to-m4pro-medium-on-november), [CircleCI support](https://support.circleci.com/en/articles/15091231-are-free-credits-available-for-macos-open-source-projects). Third-party sources put m4pro.medium (6 vCPU, 28 GB) at 200 credits/min and large (12 vCPU, 56 GB) at 400 credits/min; one credit is USD 0.0006, so about USD 0.12/min medium; a calculator estimates about 150 free minutes — [cicdcalculator](https://cicdcalculator.com/circleci-free-tier). Not confirmed on an official page. CircleCI lists Xcode 27.0 images on M4 Pro (incident page), so current Xcode images exist — [isdown incident](https://isdown.app/status/circle-ci/incidents/672919-increased-wait-times-for-macos-jobs-using-xcode-27-0-on-m4pro-large).
- Codemagic: personal accounts get 500 free macOS M2 minutes/month (resets on the 1st; no free minutes for Teams). Pay-as-you-go: macOS M2 USD 0.095/min, M4 USD 0.114/min. Third-party listings say free plan = 1 concurrent build, 120-minute build limit, 3 GB cache. Team annual macOS M2 plan USD 3,990/year — [Codemagic pricing docs](https://docs.codemagic.io/billing/pricing/), [agentdeals summary](https://agentdeals.dev/vendor/codemagic).
- Bitrise: free Hobby plan 300 credits/month, 90-minute build timeout (secondary sources); one credit = 1 Linux minute or 30 seconds on standard macOS (so about 150 macOS minutes, my arithmetic); no open-source macOS tier found; Intel macOS machines deprecated, Apple silicon only — [Bitrise pricing](https://www.bitrise.io/pricing), [Bitrise build machine docs](https://devcenter.bitrise.io/en/infrastructure/build-machines/build-machine-types.html).
- Xcode Cloud: runs `ci_post_clone.sh`, `ci_pre_xcodebuild.sh`, `ci_post_xcodebuild.sh` from a `ci_scripts` folder next to the Xcode project; secondary sources say 25 free compute hours/month with an Apple Developer membership, and report clone scripts taking over 20 minutes — [Create with Swift](https://www.createwithswift.com/custom-build-scripts-and-environment-variables-for-xcode-cloud/), [antigravitylab field notes](https://antigravitylab.net/en/articles/app-dev/antigravity-xcode-cloud-compute-hour-budget-ci-time-control-field-notes).
- GitLab SaaS macOS runners: docs (17.6) list Premium/Ultimate tiers and OSS programs, status Beta; a forum answer told a Free-tier user to wait for general availability; current docs list a `macos-15-xcode-16` image and warn of long queues because AWS bare-metal availability is limited — [GitLab docs 17.6](https://docs.gitlab.com/17.6/ci/runners/hosted_runners/macos), [GitLab docs current](https://docs.gitlab.com/ci/runners/hosted_runners/macos/), [GitLab forum](https://forum.gitlab.com/t/gitlab-pipeline-for-macos-is-stuck-for/90181).
- Third-party GitHub-Actions-compatible macOS runner services (keep the workflow, change `runs-on`): Tenki lists M4 Pro 6 CPU / 16 GB at USD 0.48/min (a figure that looks high; unverified), WarpBuild and Bitrise advertise M4 Pro runners — [Tenki docs](https://tenki.cloud/docs/runners/macos-runners), [WarpBuild](https://warpbuild.com/products/ci/macos-runners), [Better Stack comparison](https://betterstack.com/community/comparisons/getmac-alternatives/).

### Inferences
- A 15-minute build on CircleCI M4 Pro medium costs about USD 1.8 at the third-party rate; the free 30,000 credits cover about 10 such builds/month. Codemagic M2 at USD 0.095/min is about USD 1.4 per 15-minute build; 500 free minutes cover about 30 builds. Neither is verified for xnu speed.
- CircleCI, Codemagic and Bitrise all run arbitrary shell steps in their YAML configs (background knowledge, not verified in these sources), so the existing `xnu.yml` logic could be ported; the real risk is the preinstalled Xcode versions (16.2 specifically, and 26.1) and whether `sudo`, KDK download and 120-minute limits work. Not verified.
- Xcode Cloud is a poor fit: it builds an Xcode project/workflow, requires a paid Apple Developer membership, and scripts are hooks around a project build (the xnu/dxb build has no Xcode project). Inference only.
- GitLab is effectively unavailable on the Free tier; the OSS-program route would need an application.
- None of these gives independence from CI vendors; they only replace the vendor.

### Gaps
- Buildkite hosted macOS agents: not researched.
- Anka (Veertu): not researched.
- Exact Xcode versions preinstalled on CircleCI/Codemagic/Bitrise (16.2 vs 26.1): not confirmed.
- Whether CircleCI free-plan macOS credits apply to a public GitHub repository without extra approval: not confirmed.

## 4. Virtualization on Apple Silicon (Tart, UTM, Lume, Anka)

### Takeaway
On any M-series Mac, Tart or Lume (both on Virtualization.framework) can run macOS guests with specific Xcode versions, so one mini can build Xcode 16.2 and 26.x in separate VMs. Apple's license caps concurrent macOS guests at two per Mac, enforced in the kernel. Tart's ownership changed (now at OpenAI) and its license is being relaxed, a long-term maintenance uncertainty.

### Cited Findings
- Tart: Apple Silicon Mac, macOS 13.0 or later; uses Virtualization.framework; base images from `ghcr.io/cirruslabs/macos-tahoe-base:latest` (about 25 GB) — [Tart README](https://github.com/cirruslabs/tart).
- Cirrus image templates provide variants for golden-gate, tahoe, sequoia and sonoma: `vanilla` (auto-login), `base` (brew, no Xcode), `xcode:N` (a specific Xcode version plus Flutter, new images built after each Xcode release for golden-gate, tahoe and sequoia), and `macos-runner` (several Xcode versions plus `xcodes`) — [macos-image-templates](https://github.com/cirruslabs/macos-image-templates).
- The 2-VM limit: at most two macOS guest VMs run at once on an Apple Silicon host; the error is "The maximum supported number of active virtual machines has been reached". It follows section 2.B.iii of the macOS Software License Agreement (up to two additional macOS instances per Apple-branded computer, for development/testing/personal use), and is enforced in the XNU kernel via a VM quota (`hv_apple_isa_vm_quota`) — [Khronokernel](https://khronokernel.com/macos/2023/08/08/AS-VM.html). Cirrus also cited Apple's 2-VM EULA limit — [Cirrus blog](https://cirrus-ci.org/blog/2022/07/07/isolating-network-between-tarts-macos-virtual-machines).
- Tart nested virtualization: M3/M4 and macOS 15+ only, Linux guests only (`--nested`) — [Tart FAQ](https://tart.run/faq/). Disk resizing of macOS guests is manual or via Packer `disk_size_gb` — same page.
- Tart ownership: now hosted at openai/tart, license FSL-1.1-ALv2 per one source; Cirrus Labs' announcement says Tart, Orchard and Vetu will move to a more permissive license "in the coming weeks" and licensing fees have stopped; the new license was not named — [paddo.dev](https://paddo.dev/blog/ushr-launch/), [Cirrus Labs](https://cirruslabs.org/). No OpenAI roadmap for Tart found (single secondary source) — [AI IDE List](https://aiidelist.com/blog/openai-brings-tart-team-into-agent-infrastructure). Note conflicting wording: the Tart README is tagged "fair-source".
- Lume (part of trycua/cua): creates local macOS and Linux VMs on Apple Silicon using Virtualization.framework; quickstart creates a vanilla macOS Tahoe VM from an Apple restore image and connects over SSH; MIT-licensed (outside Cua Spaces and Driver Bench) — [trycua/cua](https://github.com/trycua/cua).
- ushr (a CI tool) uses Tart by default for macOS jobs and has a Lume driver as fallback — [paddo.dev](https://paddo.dev/blog/ushr-launch/).

### Inferences
- A guest macOS VM can only run a macOS version up to the host's (and Apple restore images exist for older versions subject to host support). So a Sequoia VM with Xcode 16.2 and a Tahoe VM with Xcode 26.1 both fit on one macOS 26 M1 mini, within the 2-VM limit, if run one after the other or at the same time. From background knowledge, not verified here.
- Running builds in a throwaway VM clone per job is the clean way to get ephemeral self-hosted runners and to isolate the host (see section 5), at the cost of VM overhead (a few CPU cores/GB RAM are taken by the host) and a 25 GB+ disk per image.
- The VM-per-build approach also makes native Xcode install on the host unnecessary, so the host can stay on a single macOS and be updated freely.
- The 2-VM cap is a license term, so it affects only concurrent macOS guests; this build needs one at a time.

### Gaps
- UTM: not researched (no source). Anka: not researched.
- Whether Tart's `xcode:N` tags for 16.2 and 26.1 exist in the GHCR registry (the README only gives the placeholder form); needs a registry listing.
- Whether Linux guests count toward the 2-VM quota: not addressed by the sources.
- Virtualized-guest compile speed versus bare metal (no benchmark found).

## 5. GitHub self-hosted runner on a VM or a mini

### Takeaway
A self-hosted runner is free to use with GitHub Actions and keeps the existing workflow, but GitHub says self-hosted runners "should almost never be used for public repositories". The mitigations are: no fork-triggered workflows (this repo is `workflow_dispatch` only), a JIT/ephemeral runner in a throwaway VM, and no secrets on the host.

### Cited Findings
- Self-hosted runners are free to use with Actions but you pay for and maintain the machine; they auto-update only the runner application; you update the OS and other software; they "don't need to have a clean instance for every job" — [GitHub docs, self-hosted runners](https://docs.github.com/en/actions/concepts/runners/self-hosted-runners).
- GitHub's security guidance: self-hosted runners "should almost never be used for public repositories" because anyone can open a PR; environment protection rules and required reviewers do not isolate self-hosted runners; JIT runners (created via REST API) run at most one job and are removed; reusing hardware can leak information from earlier jobs; auto-destroy is only a partial mitigation; use runner groups to limit which repositories can use a runner; keep SSH keys and tokens off the machine; least-privilege `GITHUB_TOKEN`; do not check out untrusted PR code in `pull_request_target`/`workflow_run` — [GitHub secure use reference](https://docs.github.com/en/actions/reference/security/secure-use).
- The workflow in this repo triggers on `workflow_dispatch` only and takes a `runner` input that overrides `runs-on` (so `runs-on: [self-hosted, macOS, ARM64]` can be selected without editing the workflow); jobs use `timeout-minutes: 120`; Xcode is chosen from `/Applications/Xcode_*.app` (16.2 for 15.x, 26.1 for 26.x) with `xcode-select` — [`xnu.yml`](/Users/luigicolucci/git/Bergamota/.github/workflows/xnu.yml).
- CI cache doc: `actions/cache` is used for ccache (2 GB, zstd) and host tools (0.9 MB), keyed on Xcode, DXB_SHA, macOS version and config — [`docs/CI_CACHE.md`](/Users/luigicolucci/git/Bergamota/docs/CI_CACHE.md).

### Inferences
- Only users with write access can start a `workflow_dispatch` run, and fork PRs cannot trigger it; so the "fork PR approval" concern largely does not apply to the current workflow. This is my reading of GitHub's trigger model, not stated in the fetched pages; it stays valid only while no `pull_request`/`pull_request_target` trigger is added. Any collaborator or compromised account with write access can still run code on the runner, which is why a VM or ephemeral runner is advisable.
- `actions/cache` should work on self-hosted runners (it talks to GitHub's cache service) but a persistent local `CCACHE_DIR` on the box would be faster and avoids upload/download of up to 2 GB; not verified in the docs I fetched.
- Runner labels (`self-hosted`, `macOS`, `ARM64`, plus custom labels such as `xcode-16.2`) allow routing; with one VM per Xcode version, label each VM runner. Not verified in these sources.
- Self-hosted setups keep the dependency on GitHub's control plane (job dispatch) even though compute is yours. For independence from GitHub, run the same script directly over SSH (`dxb` build script) or via a local CI such as Orchard/ushr.

### Gaps
- Official runner registration steps (`config.sh`, `--ephemeral`, `--labels`, launchd service), supported-OS table for macOS arm64 and any maximum-runners-per-host limit: not fetched (the concept page deferred to a reference page).
- Whether `actions/cache` on self-hosted runners has any behavioural difference: not documented in fetched pages.

## 6. Reducing load with caching (ccache / sccache) and what is cacheable

### Takeaway
The workflow already uses ccache for xnu's kernel C/C++ compiles (1,077 compile units) and a warm run still takes 3.4-4.7 minutes on a macos-14 runner versus 5.7-8.5 minutes cold. Assembly, linking and fixed setup are not cached. A remote cache is only useful if compiler binary and paths are identical across machines.

### Cited Findings
- `xnu/makedefs/MakeInc.def` defines `KCC = $(CCACHE) $(CC)`; ccache is set via `CCACHE=ccache`; assembly (`S_KCC`) and link are not cached; `CCACHE_COMPILERCHECK=content`; `CCACHE_SLOPPINESS=include_file_mtime,include_file_ctime`; `CCACHE_BASEDIR`/`CCACHE_NOHASHDIR` deliberately unset (base_dir rewrote `__FILE__`) so absolute paths are part of the hash — [`docs/CI_CACHE.md`](/Users/luigicolucci/git/Bergamota/docs/CI_CACHE.md).
- Measured Build step on macos-14: no cache 5:06 and 7:23; cold with 1,077 misses 8:31 and 5:42; warm with 1,077/1,077 hits 4:43 and 3:22. Fixed costs not cached: KDK download/install (about 1 min), `xnu_headers` (about 75 s), libsyscall headers, xnu clone and patch stage scripts — same doc.
- ccache supports an HTTP remote storage backend (HEAD/GET/PUT, possibly DELETE) via `remote_storage`; no direct HTTPS (use a local reverse proxy); servers listed include NGINX with WebDAV and bazel-remote — [ccache HTTP storage wiki](https://github.com/ccache/ccache/wiki/HTTP-storage). No native S3 backend in ccache (maintainer declined AWS-SDK dependency) — [ccache issue 1201](https://github.com/ccache/ccache/issues/1201).
- sccache has built-in S3 support; the endpoint can be MinIO or Cloudflare R2 — [sccache S3 docs](https://docs.rs/crate/sccache/latest/source/docs/S3.md).

### Inferences
- The cacheable share of the build is effectively the compile phase. From the doc's timings (warm 3:22-4:43 vs cold 5:42-8:31 on macos-14), compile savings are about 2-4 minutes of a 5-8 minute step on that runner, i.e. roughly 40% of the Build step; the rest is link, assembly and setup. The user's 4-vCPU Intel runner figure of about 1 hour implies a much bigger compile share there, but I did not see those timings.
- A persistent local ccache directory on a self-hosted mini is the simplest cache and removes the GitHub cache round trip; a remote ccache (HTTP + WebDAV/bazel-remote, or sccache + R2) only helps sharing across machines, and ccache needs the same Xcode clang bytes (`COMPILERCHECK=content`) and the same absolute workspace path to hit. A cache from the CI runner would therefore not hit on a different box unless both match.
- ccache still runs the C preprocessor, so a cache-hit build still needs the toolchain and SDK/KDK on the machine. The 2017 Intel Mac on macOS 13.7.8 cannot use it for Xcode 16.2/26.1 (clang binary differs, and Xcode 16/26 cannot run there).
- Distributed compilation (distcc/icecream-style) was not researched and is unlikely to help for an occasional hour-scale or 15-minute build; a faster single machine is simpler.

### Gaps
- Non-GitHub-cache timings (build on an Intel runner with ccache) and the exact share of 1,077 units in the total are not available.
- No evidence found on distcc/icecc/Bazel remote execution for xnu.

## 7. Ranking (my synthesis; inputs are the findings above)

### Takeaway
For occasional builds, rank low-cost hourly rental first (Scaleway), then AWS; for steady use, a used 16 GB M1/M2/M4 mini running Tart VMs (with local ccache) wins on cost per build and independence. Free-tier CI is a stopgap, not an independence solution.

### Cited Findings
(All underlying prices and limits are cited in sections 1-6.)

### Inferences
Scores: 1 = best, 5 = worst on each criterion; my judgement.

| Option | Cost | Setup effort | Speed | Independent of GitHub | Fits headless compile-only |
|---|---|---|---|---|---|
| Used Mac mini M1/M2 16 GB (about EUR 500-700) + SSH, optional Tart VMs | 1 (one-off; cheapest after about 3-5 rental months) | 3 (OS install, auto-login, keychain, Xcode installs or Tart images) | 2 (about 2-2.5x the 2017 i7 by analogy) | 1 (run the build over SSH; runner optional) | 1 |
| Refurbished M4 16 GB (about EUR 600-880) | 2 | 3 | 1 | 1 | 1 |
| Scaleway Mac mini M4-S hourly (about EUR 0.22/h; about EUR 5 if 24 h minimum) | 1 for occasional, 3 for continuous (EUR 149/mo) | 2 | 1 | 1 | 1 (SSH/VNC) |
| AWS EC2 Mac (USD 15-47 per 24 h minimum allocation) | 3 | 4 (Dedicated Host quota, AMI, release) | 1-2 | 2 | 2 |
| MacStadium / MacinCloud monthly (USD 99-349/month) | 4 | 2 | 1-2 | 1 | 2 |
| Free-tier CI (CircleCI about 150 min; Codemagic 500 min; Bitrise about 150 min) | 1 (small limit) | 3-4 (port workflow, Xcode versions unverified) | 2 | 5 | 3 |
| Third-party GH-Actions runners (Tenki, WarpBuild, Bitrise runners) | 3 | 1 (change `runs-on`) | 1-2 | 5 | 3 |
| GitLab macOS SaaS / Xcode Cloud | n/a (tier or project restrictions) | 5 | 2 | 4-5 | 5 |
| Cirrus CI / Cirrus Runners | Not available (shut down / closed to new customers) | n/a | n/a | n/a | n/a |

- Best total for "depend on CI as little as possible, headless over SSH, no unsupported macOS on your own Mac": buy a used 16 GB M1 or M2 Mac mini (or a refurbished M4) and run the dxb build over SSH. Use one Tart VM per Xcode version if you want Xcode 16.2 and 26.1 side by side, with a local ccache directory. Optionally register the mini as a GitHub self-hosted runner (ephemeral VM, `runner` input already supported) as a fallback.
- Best for the next few builds with no purchase: Scaleway M4-S hourly (verify 24 h minimum and image/Xcode availability first), then AWS mac2.metal for USD 15.6 per 24 h if you can do many builds in that window.
- Treat free tiers as emergency capacity only.

### Gaps
- Scores are subjective; no measured xnu timings on any of these machines. A single trial on a rental (Scaleway M4-S or AWS M1) would give the missing number before buying hardware.
