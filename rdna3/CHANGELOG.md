# 更新记录

## v0.2.21 first campaign completed (2026-11-26)

- The [summary](benchmarks/2026-11-26-v0221-summary.md) compares stock, the migrated profile and both historical optimized baselines. GR fusion is inactive for this BF16 layer split; CPU dot-x4 loses 0.89% geometrically over three alternating pairs and is not adopted. All windows restored production; no main or deployment change.

## v0.2.21 migration (2026-11-26)

- Imported upstream v0.2.21 (engine 0.1.67) and merged the adopted dev/0.2.20 RDNA3 profile on `dev/0.2.21`. All 26 frozen adopted-profile token probes and forced rollback counts match; no new failures in the six-case capped objective screen. Three-load medians: Chinese single/dual 121.00/184.53 tok/s, code single/dual 168.90/245.50 tok/s, 65% to 130% over stock. The [integration record](benchmarks/2026-11-26-v0221-integration.md) retains per-load variability and limitations. Production and `main` remain on the measured legacy baseline.

## CPU pool workers 9 vs 12 on the adopted profile (b478, 2026-11-24)

- 9 vs 12 workers on the adopted profile: arms identical at all 26 frozen probes, but 9 workers lost on every lane (Chinese single -2.67%, Chinese dual -3.65%, code dual -2.56%); 12 kept. See the [b478 record](benchmarks/2026-11-24-v0220-pool9-adapted-b478.md).

## --spec-min-p 0.35 does not qualify (b481, 2026-11-24)

- Against 0.5 on the adopted profile: Chinese single +1.96% (two of three pairs), Chinese dual +0.57%, code single -2.51%, code dual +0.63%. The lower threshold offered more guesses at a lower acceptance rate (Chinese 0.655 to 0.611). Not adopted; the profile keeps 0.5. See the [b481 record](benchmarks/2026-11-24-v0220-specminp-b481.md).

## CPU frequency governor is neutral (b480, 2026-11-24)

- `performance` against `powersave` on the adopted profile, switched only inside the candidate loads and restored after each one: Chinese single +1.76% and code single +1.46%, Chinese dual -0.75% and code dual -1.39%, with token-identical arms. Not adopted; the frequency ramp is not where the remaining single-lane time goes. The host was verified back at `powersave`. See the [b480 record](benchmarks/2026-11-24-v0220-governor-b480.md).

## v0.2.20 campaign summary (2026-11-24)

- One page for the whole line: adopted changes with their paired gains, the three-way numbers, the findings worth keeping and what remains. See the [summary](benchmarks/2026-11-24-v0220-summary.md).

## CPU pool workers 16 vs 12 on the adopted profile (b479, 2026-11-24)

- 16 vs 12 workers on the adopted profile: arms identical at all 26 frozen probes; Chinese single -3.27% (1/3); Chinese dual -2.26% (2/3); Code single -1.33% (1/3); Code dual -0.89% (1/3) — not adopted, 12 kept. See the [b479 record](benchmarks/2026-11-24-v0220-pool16-adapted-b479.md).

## RDNA hyper-connection read adopted under quality review (b477, 2026-11-24)

- `STRATA_GR_RDNA=1` with `STRATA_GR_FUSED` absent is now part of the development template, together with the gfx1100 exact subset and `STRATA_EXPDEC=1`. Three pairs, each faster in all three: Chinese single +5.61%, Chinese dual +5.03%, code single +3.84%, code dual +4.69%; TTFT 4.5% to 6.6% lower. Code dual (239.46) is above the historical production figure (235.83); the other lanes remain 4.4% to 15.7% below.
- The change alters the summation order, so it was gated by a declared review instead of token identity: the control still matches the frozen reference, the candidate differs only on the 7 raw Chinese greedy probes, sampled and code probes and the objective suite agree, and the scored texts show no degeneration. Production restoration was verified; no deployment or `main` change. See the [b477 record](benchmarks/2026-11-24-v0220-grdna-adoption.md).



- The upstream RDNA GR port aborted at the first graph capture only because `STRATA_GR_FUSED` was present in the environment (it allocates the fusion barrier during capture even when set to 0). With the variable absent and `STRATA_GR_RDNA=1` the kernels run on gfx1100.
- Diagnostic, not adopted: Chinese single +7.35%, code single +5.98% and code dual +5.50% in three pairs, Chinese dual +2.48% in the two complete pairs (one 806-token natural stop). The kernel changes the summation order: 7 of 14 raw Chinese greedy probes differ from the frozen reference (first difference at token 7 or 21), while all sampled and code probes, and the objective suite, are unchanged. See the [b476 record](benchmarks/2026-11-24-v0220-grdna-diagnostic.md).

## CPU pool workers: 9 instead of 12 (b475, 2026-11-23)

- With `--pool-workers 9` every worker is pinned to its own physical core and the engine's oversubscription warning disappears. All 26 reference probes and the 35/21 forced rollbacks held. Three pairs: Chinese dual +2.69% (faster in all three), Chinese single +2.41%, code single +0.76%, code dual -1.73% (within the 2% guardrail). Meets the adoption rule with a trade-off: about 2.5% on the Chinese lanes for a little code-dual throughput.
- Recorded as an opt-in choice; the automatic-cache template is unchanged. Production restoration was verified. See the [b475 benchmark](benchmarks/2026-11-23-v0220-pool9.md).

## Parallel batch-slot stack is neutral on this profile (b474, 2026-11-23)

- Seven parallel-stack switches (checkpoint pool, slot checkpoint move and batch-synced copy, batch GDN groups, batch attention rows, QSA slots) kept the 26 reference probes but moved every lane by less than 1.3%. Under KV streaming the engine keeps one attention launch per row, so the row merge is inactive here. `STRATA_BATCH_SELF_COMMIT` was excluded because it disables pipelined MTP. Not adopted; the gfx1100 default tables are now exhausted for this profile. See the [b474 record](benchmarks/2026-11-23-v0220-parallel.md).

## STRATA_GDN_SPLIT rejected on the exact + EXPDEC profile (b473, 2026-11-23)

- The switch kept all 26 reference probes and the 35/21 forced rollbacks, but lost median throughput on Chinese dual (-6.70%), code single (-2.32%) and code dual (-2.43%); Chinese single was flat (+0.34%). Not adopted; the profile stays at the b472 configuration. Production restoration was verified. See the [b473 record](benchmarks/2026-11-23-v0220-gdnsplit.md).

## STRATA_EXPDEC adopted on the exact profile (b472, 2026-11-23)

- Adding `STRATA_EXPDEC=1` to the four exact overrides kept all 26 reference probes and the 35/21 forced rollbacks, with no new objective failures. Three pairs: Chinese single +2.92% and Chinese dual +3.55%, each faster in all three pairs; code lanes within 1%. TTFT changes stayed within 0.7%.
- Recorded as an opt-in development profile only. Production restoration was verified; no deployment or `main` change. See the [b472 benchmark](benchmarks/2026-11-23-v0220-expdec.md).

## gfx1100 hipBLASLt long-prefill screen (b466, 2026-11-23)

- At matched expert residency, the 100401 hipBLASLt table improved diagnostic prefill rates by 4.1% at 8,192 fresh tokens and 5.5% at 16,384. Actual table dispatch was observed, but all long token streams differed across arms despite the short reference and objective checks passing. The output-preservation gate blocked formal scoring and adoption.
- No profile change. Production restoration was verified. See the [b466 benchmark](benchmarks/2026-11-23-v0220-hipblaslt.md).

## v0.2.20 integration and gfx1100 exact subset (b459–b464, 2026-11-23)

- Integrated upstream engine 0.1.66. Automatic expert-cache sizing missed the frozen 0.1.65 reference; matching actual residency restored all 26 token and finish comparisons and the 35/21 forced rollback counts. The standalone development baseline did not score because one Chinese dual request stopped at 713 tokens.
- The four-switch exact subset passed the matched-residency quality gates and the three-pair adoption rule. Chinese dual improved from 161.36 to 171.58 tok/s (+6.34%, faster in all three pairs); Chinese single fell 1.42%, within the 2% guardrail. All TTFT changes were within the 5% guardrail. The measured profile is opt-in; the automatic-cache template is unchanged.
- Recorded the pristine v0.2.20 baseline and the three-way throughput/TTFT comparison in the [b464 benchmark](benchmarks/2026-11-23-v0220-exact.md). Production restoration was verified; no deployment or `main` change.

## v0.2.19 stock baseline and three-way throughput comparison (b458, 2026-11-23)

- The pristine upstream 0.1.65 build completed two fresh loads using upstream tuning defaults, without compatibility overrides. Medians were 63.4 / 112.6 / 79.7 / 106.5 tok/s for Chinese single/dual and code single/dual. The optimized b457 build gained 44–99% against stock but remained 13–19% below the historical production 0.1.43 baseline. Production restoration was verified; no runtime configuration changed.
- Keep the three series in future optimization summaries. Reuse the production and same-version stock baselines while their identities and the measurement protocol remain unchanged. See the [b458 benchmark](benchmarks/2026-11-23-stock-vs-optimized.md).

## Reference v2 pinned on engine 0.1.65; dev/0.2.19 integration complete (2026-11-23)

- b457 self-reproducibility window: TWO full validation passes (raw 14 + sample 12 each, four fresh loads) reproduced token-for-token on all 26 probes with identical forced rolls (zh 35 / code 21) — pass-1 pinned as reference v2 (rdna3/reference-0.1.65.json, 0.1.65/0f4ae265 provenance, supersedes the b442 reference whose divergence b456 attributed upstream). The dev/0.2.19 integration is complete: merged tree behaviorally == pristine upstream, self-reproducible, objective clean, restores verified.
- Bonus two-load throughput baseline on 0.1.65 (pinned template): zh single 120.7 / dual 162.0, code single 158.3 / dual 200.4 tok/s medians — zh single at parity with 0.1.64, dual lanes 5-9% below (to be confirmed against the paired machinery in the screen rounds). All gates green before the reference pin; production restored+verified (58854970).
- Next: the gfx11 default-table screens (exact -> WMMA -> parallel) run against reference v2; the rdna port capture fix follows. See the [b457 record](benchmarks/2026-11-23-refv2.md).

## Attribution closed: the 0.1.65 divergence is upstream's, the merge carry is output-neutral (2026-11-23)

- Pristine upstream build (vendor c84f22a, zero carry, binary ce560089) vs the dev/0.2.19 merge (0f4ae265), same pins/config/seed: **zh-serial/zh-repeat/zh-pw2 byte-identical across arms** (96 tokens each), both diverging from the pinned reference at token 12 (98144 vs 97427). Predeclared verdict: upstream_source — a legitimate engine 0.1.65 numeric step; the fixed-zero graph/pool-queue carry is proven output-neutral.
- Upstream also dropped the penalty-aware pipeline (penalized requests decode serially on 0.2.19-pristine) — and the merged line's pipelined path yields identical tokens: scheduling-only, re-confirming the b421/b447 class across engine versions.
- Next: b457 self-repro window pins reference v2 (0.1.65 provenance), then the gfx11 default-table screens and the rdna port fix. Production restored+verified after all nine windows. See the [attribution record](benchmarks/2026-11-23-v0219-attribution.md).

## dev/0.2.19 integrated: merged, built, running; the reference gate holds at zh token 12 (2026-11-23)

- Upstream v0.2.19 (engine 0.1.65, 55 commits/278 files) triggered the campaign's documented reopen condition. Merged on dev/0.2.19 (vendor c84f22a + carry/fix chain to 72744d1): upstream ports supersede rdna_gr_local/rdna_kquant_lab/zero_pcie_plan (dropped); the adopted fixed-zero graph, pool queue, PLE-snap test and probes carry. Five build iterations converged (binary 0f4ae265); production restored+verified after all four validation windows.
- Two upstream-port hazards pinned for later screens: STRATA_GR_FUSED=1 (old inert template pin) makes the persistent fusion query occupancy inside the first capture (illegal, abort); the port's BF16 branch then SEGFAULTS inside the captured window graph on discrete gfx1100 -> STRATA_GR_RDNA=0 for the integration. Upstream's +7.5% decode (Strix Halo) rides this port - fixing+screening it on our card is a candidate.
- With the port off, the engine runs end-to-end: all 14 raw probes complete and the frozen reference gate fails closed exactly as designed - every Chinese probe diverges at token 12 (rolls 30->35), every code probe bitwise. Attribution pass next (suspects: the new staged HC read, verify.cpp +178, the plan-loop rework). The three NEW gfx1100 default tables (24 exact + 12 WMMA + 9 parallel switches) stay pinned off via STRATA_GFX11_DEFAULTS=0 and are the next screen batch. See the [integration record](benchmarks/2026-11-23-v0219-integration.md).

## Per-bucket profile A/B closes the campaign: the gap is pool row delivery, mapped kernel by kernel (b453, 2026-11-22)

- Paired diagnostic on the identical test config: production binary (0.1.43) vs dev binary (0.1.64) under the same strict VERIFY_PROFILE (same fused-path set disabled → comparable buckets), plus a dev LIGHT arm (fused on). Paired strict walls: dev +2.94 ms/window (zh), +3.44 (code); the net bucketed delta (+3.43/request) reconciles.
- The map: compute kernels at parity or dev-better (round 33 confirmed on today's line, no hidden kernel regression); the dominant net-positive delta is the CPU-expert row delivery — waitCPU +1.48 ms/window on stage 1, +0.97 on stage 0 (qsa +0.45..0.64) — offset by dev's 5× cheaper copy+combine (legacy front-loads rows inline; dev defers to the pool). This is the wait chain of round 33, now measured larger; the five bounded repairs of it all measured negative on throughput, and the look-ahead keeps its eroded premise. Fused paths worth ~−0.5 ms/window on 0.1.64.
- Per the predeclared rule (no new ≥0.5 ms non-chain regression bucket) **the optimization campaign closes**: ~2× stock single-lane decode, TTFT below production, quality equivalent, every lever measured-closed with a complete evidence chain. Attempts 1–4 aborted fail-closed on harness gates (0.1.43 prints no io-prefetch line and no verifier node listings; a missing --batch 2; an over-strict window-count check) — production 0.1.43 restored and verified after every attempt. A new round opens on an upstream version, model, or hardware change. See the [record](benchmarks/2026-11-22-profile-ab.md).

## MTP drafters on the front card: quality holds, the solo lanes collapse (b452, 2026-11-22)

- First round under the user-authorized quality deliberation (bar: no-worse, not bitwise; divergence is recorded evidence). Both arms ran the exact b450 binary (88c08615…, zero overlay), only STRATA_MTP_STAGE0 differed. Every deliberation gate passed: objective failure sets identical in both arms (case 19 wrong in both — the b448 flip was the truncated profile, not placement), 12-prompt real-set deliberation clean (3/12 byte-identical, ratios 0.74–1.14, no degeneration), placement proven per load (off CUDA1 / on CUDA0), off arm bitwise on the pinned reference, on arm diverging on 5/14 probes (zh rolls 30→34, recorded).
- The speed screen **rejected**: zh single **−14.81%**, code single **−4.49%** median — both beyond the −2% line — while both dual lanes improved (+5.10% / +3.11%). Frozen windows confirm the sign: front-card windows +0.95…+2.83 ms with the chain sharing the card (the configuration mtp.cpp's own comment warns about), and stage 1 waits on stage 0's hand-off, so the solo pipeline pays for the front card's contention while the bottleneck card's +517 slots only pay the batch lanes. The b447 +3–6% solo ceiling is refuted; the placement axis is now measured in all three configurations and closed. STRATA_MTP_STAGE0 stays 0; template unchanged; production 0.1.43 restored and verified. See the [record](benchmarks/2026-11-22-stage0-auto.md).

## MTP draft stream at the lowest priority: solo lane pays, screen fails (b451, 2026-11-22)

- First code-level screen on the 0.2.18 line: a 7-line mtp.cpp overlay (reverted after the screen) turned `STRATA_MTP_HIP_PRIO` into a level — negative maps to the range's lowest end (gfx1100: `1..-1`) with a `lowered` announcement. Binary `b78af7f2…` from a clean overlay-verified build (base/patched hashes pinned in provenance). All quality gates passed: both arms bitwise-reproduce the pinned reference (rolls 30/22), objective unchanged, graph signatures identical between arms — the cross-arm graph-equality gate is restored for priority-class screens because a stream priority moves neither weight nor capacity.
- The predeclared screen **failed**: no workload faster in all three pairs (zh single −1.44% median, zh dual +2.00%, code single +0.06%, code dual +10.52% — each dual lane lost one pair). The frozen-load window diagnostic explains why: lowering the priority makes solo windows ~0.1 ms/window SLOWER (zh pw2 +0.13, forced +0.12) — the deferred chain delays the next stage-0 launch that waits on it, the two-orders-smaller mirror of b421's raised arm (+1.08 ms). The contention is asymmetric; the default interleaving is already near-free. **All three priority levels are now measured — the axis is closed, the template keeps the switch unset.**
- Two windows aborted fail-closed along the way, both recorded: a gate-design fix (this template loads THREE drafters — solo + two `--batch-mtp` slots — so a healthy on-arm announces three identical lines, not one), and a first-of-its-kind spontaneous early stop (zh solo stopped naturally at 395 tokens under live adaptation; no reroll by design). Production 0.1.43 verified restored after every window. See the [screen record](benchmarks/2026-11-22-mtp-prio-low.md).

## dev/0.2.18 validated: bitwise-identical to dev/0.2.17, first screen at parity (2026-11-22)

- The vendor merge (engine 0.1.64) completed with ZERO conflicts and the validation confirmed the prediction: all 26 frozen probes bitwise-identical to the pinned 0.1.63 reference (the 0.1.63→0.1.64 bump is behavior-transparent for this workload — upstream 0.2.18 adds the Hy4-Flash model family without touching the Qwen3.8 decode path). Two-load screen at parity: Chinese single/dual 121.5/170.1, code single/dual 166.7/220.8 tok/s medians; no regression. Production 0.1.43 verified restored. See the [integration record](benchmarks/2026-11-22-v0218-integration.md). Template metadata: 0.2.18-rdna3.1 / 0.1.64.

## dev/0.2.17 validated: the reference divergence chased to upstream #6110, first screen at parity (2026-11-22)

- Six measured bisect windows isolated the frozen-probe divergence vs the 0.2.16 baseline to ONE upstream line: the #6110 stage-trim fix (`trim = ... && !stage_trim`) — 0.2.16 trims stage weights twice on a two-card split, 0.2.17 once, and with `--trim-stage-weights` set that changes the residency the run starts with. With that single line restored to 0.2.16 the ENTIRE integration (upstream v0.2.17 + b437–b441) reproduces the adopted baseline bitwise (14 raw + 12 sampled probes, forced rolls 32/23). Excluded with measured runs along the way: the rope HIP rounding hunk, all 10 changed GPU kernel files, b437's own restructure, verify.cpp/mtp.cpp.
- The completion run (b443) on the integrated binary passed every gate: self-reproducibility (a fresh load reproduced the first run token-for-token on all 26 probes), internal consistency, objective (same five shared failures), graph structure. Two fresh-load scored screens: Chinese single/dual 125.9/163.7, code single/dual 162.1/200.2 tok/s medians — parity with the adopted 0.2.16 arm within this rig's pair spread. Production 0.1.43 verified restored after every window.
- Next candidates on the new line: `STRATA_POOL_DEP` (upstream-measured −2.9% window, bit-exact), `STRATA_SPLIT_CPU_SHARE`, `STRATA_SLOT_READ_WIDE/_AHEAD`, the P2P pair. See the [integration record](benchmarks/2026-11-22-v0217-integration.md).

## dev/0.2.17: upstream v0.2.17 integrated, engine 0.1.63, measured baseline pinned (2026-11-22)

- Imported upstream v0.2.17 (engine 0.1.63; 339 changed / 389 added / 14 removed files vs v0.2.16) on top of the five experiment commits b437–b441. Manual conflicts were five files: `expert_source.cpp` (kept the b437 planner wrap; upstream's peer-P2P scatter tail and the #6023 peer-resident note ride along), `verify.cpp`/`verify.hpp`/`generate.cpp` (CRLF-vs-LF whole-file conflicts; re-merged on normalized content — zero genuine conflicts in verify.hpp/generate.cpp, two in verify.cpp resolved keeping the RDNA3 `log_hip_graph_nodes` refactor + upstream's STRATA_L2_PF block and the #5636 pending-error drain), and `expert_source.hpp` (both field sets kept).
- Upstream 0.2.17 defaults that shift the measured baseline are pinned OFF in the template pending their own screens: `STRATA_POOL_DEP=0` (dependency-batched pool schedule) and `STRATA_SPLIT_CPU_SHARE=0` (CPU prefill share armed by default on a two-card split). `STRATA_ADAPT_FETCH=2` was verified CC12-only (gfx1100 unaffected); `STRATA_ROUTE_TAIL_SKIP=0` stays as the now-upstream default; L2 prefetch is CUDA-only at runtime.
- Template deployment metadata: `0.2.17-rdna3.1`, engine 0.1.63, revision `v0217-shstream-aux-pool12-zero-pcie`; `source-snapshot.json` records the v0.2.17 API-tarball provenance (codeload unreachable) and the 28 local engine changes. Build and paired validation on 66 to follow in the integration record.

## Shared-expert gate/up pair: probe-pinned but coverage too small to screen (2026-11-21)

- Extended `native_mmvq_pair` to IQ4_XS gate/up pairs behind default-off `STRATA_LAB_RDNA_SH_PAIR`. The probe pinned the real boundaries: bitwise vs both solo kernels for even `n_out` at cols 1..8 (including captured replays and the discriminating 642 shape), a ~1e-7 relative difference at odd `n_out` (guard falls back), and a pre-existing lab-family pair inconsistency at 642 (recorded, not gated). Frozen token/rollback/sampled/objective gates all passed with the pair active. Measured coverage is only 4 front + 2 final layers (−12 nodes per window pair, ~0.7%), far below the node-analysis estimate, and the scored screen aborted on one early-EOS burst — the switch stays off; see the [record](benchmarks/2026-11-21-sh-pair.md).
## Shared-expert fork stream confirmed on the fixed-zero template (2026-11-21)

- First paired screen of `STRATA_SH_STREAM` on the current template (carried =1 since r28, never A/B'd there; production runs the HIP default off). The fork arm won every workload median (+3.78% / +0.09% / +2.07% / +3.39% for zh/code single/dual) with TTFT medians down 2.4–3.4%, so the predeclared removal rule did not fire and the template keeps `=1`. The fork's extra per-layer quantize (qdedup is fork-exclusive) does not outweigh its cross-stream overlap on this dual-card doorbell path; the single-GPU HIP evidence in the engine comment does not transfer. See the [paired record](benchmarks/2026-11-21-sh-stream-ab.md).
## Pinned penalty-history upload rejected (2026-11-21)

- Staged the solo pw2 penalty upload through page-locked memory and removed the full-stream synchronize that protected the pageable buffer. All frozen-output gates passed byte-identically (token IDs, sampled repeats, continuation, graph histograms). The service pairs rejected it: Chinese single/dual fell in every pair (−5.81% / −4.68% medians); the sync was useful pacing, not a bottleneck. Host-blocking at this upload is ruled out of the remaining window gap. Switch stays off; see the [paired record](benchmarks/2026-11-21-pin-penalty-hist.md).
## PLE history snapshot fusion stays off (2026-11-21)

- Combining history advance and snapshot removed one node per snapshotted row on the front stage. Both HIP probes and frozen token, rollback, and sampled continuation checks passed. Three fresh-load pairs changed median decode or batch throughput by +4.03% / −0.18% / −2.24% / +1.70% for Chinese single/dual and code single/dual. No lane improved in all three pairs; the development template is unchanged. See the [paired record](benchmarks/2026-11-21-ple-history-snap.md).
## Fixed-zero PCIe host planner (2026-11-21)

- Added a default-off indexed routing planner for eligible grouped host plans. Frozen token IDs matched and both stages published plans faster, but the three service pairs did not show a general gain (Chinese single −1.70%). Keep the template unchanged. See the [paired record](benchmarks/2026-11-21-zero-pcie-plan.md).
(PLE history advance and snapshot fusion: one node fewer per row, no service gain (off))
(Pinned penalty-history staging: removing the post-upload stream sync costs the Chinese lanes (off))
(Shared-expert fork stream confirmed on the fixed-zero template (keep STRATA_SH_STREAM=1))
(Shared-expert gate/up pair launch: probe-pinned, coverage too small to screen (off))

## Fused-A8 port rejected on the real binary (2026-11-21)

- Redid the fused-vs-split HC ablation on the real production binary (`58854970…`, its own runtime config; the earlier attempt had unknowingly tested an upstream 0.1.62 package with no legacy HC kernels): split costs ~0.3 ms/window on Chinese (service −4.0% single / −1.8% dual) and nothing on code. **The fused-A8 single-launch port is rejected** — a grid-barrier port into overlapping development graphs is not worth one lane's ~2%. See the [record](benchmarks/2026-11-21-real-fused-ablation.md).

## CORRECTION: no production incident; official arms were measuring an upstream package (2026-11-21)

- The same-day campaign's "production at half speed" conclusion was wrong: the campaign preflight's engine path has pointed at the 2026-11-19 update package (an upstream 0.1.62 build, `bdec5453`) since 11-18, and that binary has always measured ≈57–75 single-lane under this methodology (matching the R26 stock row). A four-lane probe of the live service (real 0.1.43, `58854970…`) measured 139.5 / ~204† / 188.3 / ~269† — its historical speed. The same-day pairs stand as dev-vs-stock (+21% to +117%); dev trails real production by ~11–14% on single lanes. Preflight engine paths are no longer trusted for production-representative arms. See the [corrected record](benchmarks/2026-11-21-legacy-dev-sameday.md).

## File-tier prefetch off (2026-11-21)

- `STRATA_IO_PREFETCH=0` also disabled routing look-ahead on the tested mapped `experts.bin` configuration. Across three fresh-load pairs, Chinese single/dual and code single/dual throughput changed by −2.21% / +0.14% / −2.12% / +3.03%. Frozen texts matched across arms and modes. The template is unchanged; see the [paired record](benchmarks/2026-11-21-file-tier-prefetch-off.md).

## Residency-upload deadlock fixed; local Q8 rejected (2026-11-21)

- Root-caused the flaky layer-24 stall: the adaptive tier's thread called the residency upload's blocking legacy-stream `cudaMemcpy` beside pipelined windows — a host↔GPU circular wait with the window spin kernels (reproduced 3/3 with a scoring-then-load recipe; captured by gdb before the watchdog). `res_put` now uploads async on a per-device stream and only waits in serial-window mode; six recipe cycles clean, frozen four-mode parity byte-identical.
- The freed-up three-pair Q8 confirmation: −4.5%/−3.8%/−2.4%/+0.2% — **local Q8 rejected**; BF16 stays the default. See the [resolved record](benchmarks/2026-11-21-hc-q8-resolved.md).

## Local HC Q8 confirmation blocked (2026-11-21)

- The three-pair confirmation of local Q8 HC weights on the adopted baseline could not run: 2 of 8 Q8 loads deadlocked at the layer-24 CPU-expert dispatch and were killed by the watchdog with an identical signature; three targeted reproductions (fresh load, two-load sequence, exact warmup body) ran clean, so the race is flaky. The one completed pair shows no clear gain. Local Q8 stays off until the deadlock is root-caused. See the [blocked record](benchmarks/2026-11-21-hc-q8-blocked.md).

## Pool compute queue / async C (2026-11-21)

- Implemented the pool-serve redesign as `STRATA_LAB_RDNA_POOL_QUEUE=1` (default off): a lock-free 4-slot SPSC ring feeds one runner thread that is the pool's only caller and raises C per ring. Frozen texts byte-identical; throughput −9.0%/−4.8%/−10.4%/−5.7% (medians) — **not adopted**. The inline synchronous design is confirmed the best serve model tested on this rig; the remaining gap is structural window scheduling. Also documented an unexplained `std::system_error(EPERM)` from `pthread_mutex_unlock` in a HIP-build worker thread (the reason the ring is lock-free). See the [record](benchmarks/2026-11-21-pool-queue.md).

## Legacy fused-A8 ablation attempt (2026-11-20)

- Ran the production 0.1.43 binary with `STRATA_GR_FUSED` 1 vs 0 under the frozen protocol: inconclusive, because 0.1.43 does not reproduce texts across identical requests on this rig while the development engine does (a role reversal of the R14–R20 instability). The HC fused-A8 port is deprioritized against the pool serve model's larger 2.4 ms/window wait chain. See the [attempt record](benchmarks/2026-11-20-legacy-fused-ablation.md).

## Light profile of the adopted baseline (2026-11-20)

- Single frozen load with `STRATA_VERIFY_PROFILE=1` + `STRATA_VERIFY_PROFILE_LIGHT=1` on the adopted template binary: bucket-level determinism to 0.01 ms; waitB/PCIe ≈ 0.05 ms confirms the fixed-zero graph; the surviving host wait chain is ~2.4 ms/window (stage-0 waitA ~1.0 + stage-1 waitCPU ~1.7); HC reads ~2.9 ms/window combined. Ranks the pool serve model as the next target, then the HC fused A8 port. See the [diagnostic record](benchmarks/2026-11-20-light-profile.md).

## MTP draft stream priority screen (2026-11-20)

- Verified gfx1100's stream priority range (1..-1) with a live probe, then screened `STRATA_MTP_HIP_PRIO=1` on the dual-card template: Chinese dual +3.83% in every pair, Chinese single −2.63% median, code lanes flat; frozen texts byte-identical. Not adopted. Added a diagnostic log line to `mtp.cpp` that reports the applied priority and range. See the [screen record](benchmarks/2026-11-20-mtp-prio.md).

## Routing look-ahead off screen (2026-11-20)

- Tested `STRATA_LOOKAHEAD=0` on the dual-gfx1100 template: Chinese dual improved in all three pairs (+8.03% median) but code single regressed in all three (−2.17% median); frozen serial/pipelined/forced-rollback texts stayed byte-identical across arms. Not adopted; the look-ahead stays on. A first campaign attempt aborted on its own config assertion before scoring and production was restored automatically. See the [screen record](benchmarks/2026-11-20-lookahead-off.md).

## Fixed-zero expert PCIe verifier graph (2026-11-20)

- Added a default-off HIP/gfx1100 experiment that omits the expert PCIe branch from verifier graphs only on a fixed-zero two-card split. Cross-card activation hand-off and ordered DMA publication remain unchanged.
- Confirmed 156/132 fewer nodes on the two stages, exact token IDs in the frozen checks (including forced rollback and batch), and a working nonzero-share fallback. The ordered-flag test passed on both cards.
- Three fresh-load pairs at 1,024 output tokens improved median Chinese single/dual and code single/dual throughput by 3.7%/7.1%/4.6%/1.2%. Enabled in the fixed-zero dual-card development template (`shstream-aux-pool12-zero-pcie`), not in the engine defaults. Production 0.1.43 was restored without changing its configuration. See the [validation and measurements](benchmarks/2026-11-20-zero-pcie-graph.md).

## dev/0.2.16 异步C设计与驻留/页缓存假设关闭（2026-11-20）

- 架构分析(无新测速):ExpertPool是串行批机器——serve线程自身参与相计算、批状态复用,双批次并发需要双池;异步C因此退化为"每stage独立serve线程+独立池"的结构项目。驻留率两版持平(21785对21636槽),dev的miss读全部页缓存命中(0存储读)——驻留与页锁补充假设均不成立。
- 结论:dev流水线使单窗口CPU专家需求相对现役串行翻倍,等待是这需求的调度伪影;重排(r37)与量化(r41)已被数据否决,剩余唯一路径为多线程serving(每stage独立serve线程+池),规格见`benchmarks/2026-11-20-round42.json`。
- 本轮无源码改动;现役未动。

## dev/0.2.16 池trace工具与服务量化（2026-11-20）

- 新增:serve路径的`STRATA_VERIFY_TRACE`请求尾转储(此前仅超时转储)——池逐ring握手面包屑成为常规诊断工具。trace证实:四verifier竞争(pw2每级两个),服务节奏约0.5ms/ring,存在单verifier贪心连做20-24个ring(约5ms)的突发,期间另一级干等——即stage-1 waitCPU的饿死机理。
- 新增默认关闭的`STRATA_LAB_RDNA_SERVE_QUANTUM=N`(每次service()至多服务N个ring):q4单路+5.0%但双路-3.4%/代码单-2.0%,q8全负,**不采纳**——平滑突发以双路批处理换单路时延,不划算。
- 至此三个有界调度修复(重排r37/直抬B r37/量化r41)全部以数据否决;等待链(+0.82ms)的闭合需要跨窗口工作预跑的serve模型重设计,为专门工程。记录见`benchmarks/2026-11-20-round40-41.json`;测后现役恢复。

## dev/0.2.16 串行分解与分割平衡收官（2026-11-20）

- 第38轮(无新测速,取自r35日志):dev串行窗口31-33ms的首次完整分解——池等待13.5ms、**图执行本身约14.6ms(无计数器覆盖,串行暴露/流水线隐藏)**、commit/emit 0.79+draft 1.3+adapt 0.3-1.5(现役全为0且其等待全部内嵌于GPU-reach wait)。流水线模式的verify内仍有约3.1ms未归因项。结论:等待链修复必须让池在ring之前预计算(跨窗口前瞻),重排类修复已被第37轮证明不足。
- 第39轮:layer-split平衡筛选——24/24双路变差(145.2/199.3),28/20引擎SIGABRT崩溃,**26/22保持最优**,该方向关闭。
- 记录见`benchmarks/2026-11-20-round38.json`、`round39.{md,json}`;测后现役恢复。

## dev/0.2.16 重叠调度有界修复未过确认（2026-11-20）

- 机制定位:waitB来自`raise_flag_b_ordered`的粘滞`copy_used_`(空计划也要等拷贝流排空),stage-1 waitCPU来自泵序D→A→B(新窗口)→s1(老窗口)的饿死;现役双卡实为串行解码(min_st=3)。`--pipeline-windows 0`在dev单路崩至60/75,不可用配置复现。
- 新增两个默认关闭开关(构建`f5d5838`):`STRATA_LAB_RDNA_B_DIRECT`(以已执行计数判定可直抬,#5096危害仍覆盖)与`STRATA_LAB_RDNA_PUMP_AGE`(老窗口stage-1优先)。R36初筛pumpage四路+2.8~8.8%,R37 1024×3确认全部回落(-2.5/+0.6/-0.8/-0.3%),**均不采纳**;单次768筛选±4%臂位漂移再次得到验证。
- 闭合等待链(+0.82ms)需跨窗口池预跑(look-ahead),属流水线结构改动,列为后续专项。记录见`benchmarks/2026-11-20-round35-37.md`;三轮测后现役恢复。

## dev/0.2.16 HC量化重筛收官（2026-11-20）

- 第34轮在已采纳组合上重筛HC Q8/Q8+A8(现役生产即Q8+A8,dev移植版为BF16):hcq8混合(-2.2/+2.5/-1.8/0),hcq8a8全负,均不采纳;核对源码确认`STRATA_GR_FUSED`屏障仅作用于本身已更慢的A8读,接通亦无法翻盘。HC差距(+0.38ms/窗口)归入内核launch/占用级专项。
- 至此34轮内配置/参数/单路径候选全部筛完并留证;剩余为两项代码级专项:generate.cpp窗口重叠调度(+0.82ms等待链)与HC内核launch差异(+0.38ms)。记录见`benchmarks/2026-11-20-round34.md`;测后现役恢复。

## dev/0.2.16 服务态轻量剖析与最终归因（2026-11-19）

- 新增`STRATA_VERIFY_PROFILE_LIGHT=1`(构建`75a1968c`):保留阶段时间戳但八处融合门控改为`prof_blocks_fusion()`,生产执行不变;补上插桩会关融合导致服务态无法归因的缺口,保留为后续标准工具。
- 服务态阶段表对现役插桩表(中文主阶段):等待链+0.82ms、HC读取+0.38ms为仅有的两块差距;计算核−0.16ms、copy+combine−0.19ms开发版反超(convab融合与SH_STREAM的服务态效果首次可见:ab 0.20→0.04、shared+quant 0.65→0.04)。
- 至此差距归因闭环:后续专项为generate.cpp窗口重叠调度(打+0.82ms的CPU专家等待)与HC内核launch/占用差异(+0.38ms)。记录见`benchmarks/2026-11-19-round33.md`;测后现役恢复。

## dev/0.2.16 窗口效率旋钮终筛（2026-11-19）

- 第32轮筛`--pipeline-windows 3`、`--spec-min-p 0.45`、`--mtp-max-t 4`、`--spec 5`:全部为负或混合,现有参数(pw2/spec4/min-p0.5/max-t3)确认为该构建最优。结合第29轮回滚率与每窗口token数两版持平的观察,**开关与参数层的空间至此清零**;剩余差距(约−15%/−12%/−18%/−10%)全部在每窗口重叠调度成本,后续唯一方向为generate.cpp窗口重叠设计本体的专项移植/重设计。
- 记录见`benchmarks/2026-11-19-round32.md`;测后现役恢复。

## dev/0.2.16 服务态等待归因与路由发布融合移植（2026-11-19）

- 第29轮仅开`STRATA_DECODE_TIMING`(融合路径不变)对照:开发版窗口16.18ms vs 现役13.41ms,**GPU-reach wait 11.80 vs 10.09(+1.71ms)为最大差距块**,host plan +0.17ms次之,开发版CPU切片反而略低——等待链差距在重叠设计而非CPU计算本身。
- 移植现役路由发布融合:新增默认关闭的`STRATA_LAB_RDNA_ROUTE_PUBLISH=1`(构建`4bd26459`),多token top-10与resident doorbell一次发射;仅普通路径接管,resident-plan/doorbell-store/always-publish/route-prior等保持原路径,shared-expert晚分叉流序不变。
- 1024×3对照基本持平(中文双+1.1%其余噪声内),一次内容级提前结束(844/1024)记录,**不采纳**;冻结跨臂串行文本逐位一致,融合与分离路径bit级等价,正确性留档。找回单个旧路径仍不等于找回现役重叠设计;后续剩余方向为窗口重叠调度整体与HC内核残余。
- 记录见`benchmarks/2026-11-19-round29-31.md`;测后现役恢复。

## dev/0.2.16 草稿层HC融合确认收口（2026-11-19）

- 第28轮以第27轮最终构建(`5614920c`)1024×3确认第25轮筛选候选:`STRATA_QFUSE_HC=1`各路噪声内(对照114.08/155.18/148.84/212.62,-1.6%/+0.2%/+0.2%/-0.2%);Q32+QFUSE_HC为-1.3%/-0.5%/+1.9%/+2.0%,代码两路受益但中文单路退化,均不采纳,维持默认关闭。开关级候选至此全部筛完。
- 增加英文`rdna3/README.md`总览(与中文日总结互补);工作树来源表述统一为`cennac-rdna3-dev`分支。无源码改动,所有流达到1024预算,测后现役恢复。见`benchmarks/2026-11-19-round28.md`。

## dev/0.2.16 CPU结果合并迁移与全天总结（2026-11-19）

- 确认开发版继承的 `STRATA_COMBINE_CPU=1` 没有实现；新增独立默认关闭的 `STRATA_LAB_RDNA_COMBINE_CPU=1`，直接合并CPU映射结果与GPU命中结果，省去复制／命中加和，保留原等待与shared gate。限制适用路径并保留回退，非HIP后端不引用新API。
- 两卡各600组逐位／图重放／哨兵检查和240次非法参数拒绝零失败；最终探针补全零CPU／GPU／shared边界并重测通过。冻结专家自适应和后缀草稿后，对照／实验两组中文与代码的串行重复、流水线、强制回滚文本均一致。有限三题冒烟均2/3，不宣称完整质量通过。
- 第26轮1024×3：对照中文单／双117.70／163.49、代码单／双159.60／212.89；CPU合并115.40／162.78、159.20／214.11 tok/s，约−2.0%／−0.4%／−0.3%／+0.6%，没有全面收益，保持默认关闭。最终平台隔离改动没有改变HIP引擎二进制哈希，第27轮另做确认与冻结验证。
- 汇总已采纳的惩罚流水线、BF16 HC、shared stream、aux CPU放置与pool12，以及未采纳实验、质量边界和剩余等待链差距，见 `benchmarks/2026-11-19-summary.md`。开发版仍未追平现役，不合入main；两轮测后恢复现役，配置／运行中二进制核对通过，所监测AER增量及GPU故障为0。

## dev/0.2.16 BF16 HC 32列核与调度边界验证（2026-11-19）

- 新增默认关闭的 `STRATA_LAB_RDNA_GR_Q32=1`：BF16 HC up每组32列，可在没有全局计数器时融合q8_1图像；保留逐行求和顺序，没有跨工作组自旋。Q8/A8路径不变。
- 保留调用方的融合策略，禁止把未写入图像误报为已生成。探针经实际调度入口检查T=1…8、apply/inject、零值、图重放及关闭融合后的回退；两卡共1600字段、192图像／哨兵、132图重放、256调度返回值及64未写入检查零失败。
- 第24轮1024×3：对照中文单／双115.80／162.84、代码单／双156.50／216.59；Q32为116.60／159.85、157.80／213.72 tok/s，双路略慢，不设为默认。两组冻结串行重复／流水线／强制回滚本轮均一致，三题冒烟仍2/3；此前重复输出不稳定的诱因仍未定位。
- 第23轮显式绑核及tasks13均无全面收益，未采纳；末尾对照显示约3%会话漂移，保留否定结果。全部详细速度、首字、资源、失败与恢复记录见 `benchmarks/2026-11-19-round23.md`、`round24.md`。
- 第25轮单独打开草稿层 `STRATA_QFUSE_HC=1`，确认Q32的无计数器图像入口实际生效；768输出初筛119.40／159.72、157.60／214.25 tok/s，代码双路持平，没有全面收益，保持默认关闭。三轮均恢复现役，原配置／运行中二进制哈希一致，所监测AER计数无新增。

## dev/0.2.16 CPU池扩容采纳与TSUM崩溃记录（2026-11-19）

- `--pool-workers` 8→12 于1024×3确认:中文单115.63(+5.0%)、双162.13(+3.4%)、代码持平;pool16不如12,swiglu代码-7.9%否决,lfuse混合未采纳。模板更新为 `shstream-aux-pool12`。
- **`STRATA_TSUM=1` 在 gfx1100 引擎加载期 SIGABRT 崩溃**(exit -6),含TSUM的组合未测;保留为否定证据,勿在gfx1100启用。
- 本日三次采纳(SH_STREAM、AUX、pool12)累计交错对照:zh单+6.9%、zh双+5.5%、code单+4.4%、code双+1.1%;对现役剩余差距约−17%/−13%/−22%/−10%。
- 记录见 `benchmarks/2026-11-19-round21-22.md` 及JSON/实验计划;无源码改动,测后现役恢复。

## dev/0.2.16 SH_STREAM＋AUX_CPUS 采纳与一致性检查失效发现（2026-11-19）

- 按第十三轮归因筛选调度/放置类开关并确认：`STRATA_SH_STREAM=1`＋`STRATA_AUX_CPUS=auto` 于1024×3对照中文单112.36（+3.9%）、双164.27（+6.9%）、代码单152.57（+4.5%）、双212.18（+0.7%）。两开关不改浮点算术，模板已更新（`shstream-aux-stage1`），对现役差距收窄至约−16.5%/−7.4%/−16.4%/−7.5%。
- copyprio（本轮−0.5/−0.9%代码路）与 coldov4 未采纳；shstream 单独亦未采纳（zh双仅+1.1%）。
- r16-shstream 一次内容级提前结束（977/1024，`stop`）按设计记录并中止该战役；连同第九轮事件共两次，均为SH臂，继续监控。r17 确认十二流全部有效。
- **方法论发现：当日下午起冻结串行重复检查在本机对纯对照配置也失败**（热/冷加载共4组对照、6/6中文），与开关无关；此前各轮以该检查作否决依据的结论作废（exact1 的否决仍以其更慢独立成立），诱因未定位，列为开放问题，恢复可复现前该检查不用于开关甄别。
- 无源码改动；记录见 `benchmarks/2026-11-19-round15-17.md`、`round18-20.md` 及同名JSON/实验计划；测后现役恢复。

## dev/0.2.16 冻结归因与CPU分发对照（2026-11-19）

- 核对本地工作树两候选：`STRATA_IQ3S_EXACT1`已被上游收编为`STRATA_CPU_IQ_ALL_EXACT1`（五格式ggml逐位路径，默认关），其速度收益经`STRATA_IQ_MT_MIN=1`已在当前组合兑现；`STRATA_PIPED_ADAPT_EVERY`的上游等价物在layer-split上被排除，不移植。对照审计见`validation/2026-11-19-switch-audit.md`。
- 第十三轮冻结插桩逐阶段对照（512输出单路）：现役重建10.27/10.87、开发版12.20/12.81 ms/窗口；开发版慢约1.9ms，约45%为等待链（waitB+PCIe grp+waitCPU 0.21→1.04），18%为HC读取残余，copy+combine开发版更好。剩余差距指向未移植的重叠调度与HC内核，不是CPU分发内核。
- 第十四轮768输出对照：MT_MIN=1对照组中文单/双108.8/155.3、代码143.4/201.8 tok/s；ALL_EXACT1臂−3.4%/−3.9%/−4.1%/持平且中文冻结串行重复文本不一致（nt=1与成组舍入不同，违反窗口不变性），不采纳；无MT_MIN的ggml臂更慢，确认MT_MIN=1最优。
- `iq_avx2_parity`在本机CPU全量零失败（含exact行）；exact1开关生效日志、引擎阶段表、逐次数据保留在`validation/round14-*`与`benchmarks/2026-11-19-round13/14.json`。无源码改动，模板与main不变，测后恢复现役。

## dev/0.2.16 分格式迁移与MTP窗口核对（2026-11-19）

- 增加默认关闭的K-quant实验类型选择，支持只启用模型内部的IQ4_XS／Q6_K投影；空值、非法列表或未知类型完全回退，不更换IQ3_XXS权重。
- 十个独立进程共4000组数值检查及100组图重放通过；未选格式与上游逐位一致，补上实际选中格式日志。微基准计时改为显式开启。
- 1024输出、每项三次：中文单／双路115.4／155.9→111.7／160.3，代码154.8／211.5→156.3／216.8 tok/s。中文单路退化，没有成为新的全面更优组合，默认模板与main均不变。
- 将普通解码窗口计数与每组六条单路请求逐条对上：中文窗口耗时17.23→17.14ms，但窗口数515→532，更多窗口抵消局部计算收益；不能将短测+3.5%宣称为稳定提升。
- 冻结条件下中文／代码串行重复、流水线及强制回滚文本一致；三题冒烟仍均2/3，订单算术失败。测试后核验现役二进制、原配置和端口恢复，未新增AER或GPU故障。


## dev/0.2.16 阶段定位与K-quant迁移（2026-11-19）

- 对现役和开发版做GPU阶段计时；区分诊断模式与正常服务，不将插桩成绩当作生产速度。
- 完成共享专家流、复制优先级、专家SwiGLU融合筛选，保留一次提前EOS；没有改变默认模板。
- 核对实际权重，补上IQ4_XS、Q6_K、IQ3_S及适用Q4_K/Q5_K的main稠密投影核，新增默认关闭的`STRATA_LAB_RDNA_KQUANTS=1`，仅gfx1100进入。
- 两卡各400组数值、边界、列一致性、成对发射与图重放通过，最大相对L2误差`1.69e-7`；冻结条件下中文、代码的串行重复／流水线／强制回滚文本一致。
- 同构建三次对照：中文单／双路113.9／155.1→111.3／162.9，代码152.0／210.3→156.2／220.6 tok/s。双路约+5%，中文单路有退化，保留独立开关，未合入main。
- 三题格式／算术冒烟两组均2/3，不能宣称完整质量通过。详细速度、首字、MTP接受率、资源与失败记录见第八至十轮。

## dev/0.2.16 后续实验（2026-11-19，未改变默认组合）

- 接通复用上游Q8_0权重的HC Q8/A8读取，修正探针对未定义`lo`工作区的比较，保留初次失败证据。没有额外分配旧R8权重；没有启用跨工作组自旋屏障。
- 迁移保持上游求和顺序的small-format wave32布局，并为实际Q5_K输出头增加独立实验开关；两卡各400组逐位检查及图重放通过。
- 记录融合、行分组、Q8/A8、精确MMVQ与输出头布局的筛选及三次复测。Q6开关因实际输出头为Q5_K而未生效，明确保留为无效开关重复对照。
- Q5_K头局部4列耗时下降27.5%，整机仅约±1%，没有确认全面收益，不替换默认模板。该轮中文111.9／156.3、代码152.9／212.6 tok/s（单路decode／双路整批）。
- 冻结条件下中文与代码的串行重复、流水线和强制回滚文本一致；有限格式冒烟仍有算术失败，不宣称完整质量通过。全部实验完成后恢复现役，未合入main。
- 复现开关、数值探针、源码哈希和各轮速度／首字／资源记录见`experiments/`、`validation/`与`benchmarks/2026-11-19-round*.md`。

## v0.1.43-rdna3.1（legacy-20261110）

- 以实际运行的 engine 0.1.43 优化版作为主分支基线。
- 汇总 RDNA3 计算核、单请求流水线、双请求调度、MTP、专家驻留刷新、会话复制等现役改动。
- 收拢实际构建采用的源码覆盖层和依赖，避免依赖机器上散落的旧静态库。
- 保留上游及参考实现的许可证和来源说明。

## v0.2.16-rdna3.1（开发中，engine 0.1.62）

- 在正式版源码上增加可选的惩罚历史流水线支持。
- 筛选草稿设备、草稿长度、硬件队列数量、HC v3 和 HC 量化。
- 实验迁移 BF16 RDNA3 HC read；尚未达到主分支性能，不合入 main。
- 详细结果、提前 EOS 和一致性检查限制见各轮测试记录。

本开发分支已纳入惩罚历史流水线及BF16 HC read源码、数值探针和验证输出。最佳组合中文单路113.2、双路合计156.9，代码单路150.9、双路合计208.3 tok/s；未达到main，不合并。
