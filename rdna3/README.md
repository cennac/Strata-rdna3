# rdna3 — dev/0.2.22 (upstream v0.2.22, Qwen engine 0.1.69)

Latest: the [v0.2.22 benchmark and optimization summary](benchmarks/2026-11-26-v0222-summary.md) records stock and optimized three-load medians, frozen token compatibility and the inactive RAM-reservation setting on this profile. There is no established extra speed gain over the prior optimization line; production and main remain unchanged.

The [first v0.2.21 campaign summary](benchmarks/2026-11-26-v0221-summary.md) records the accepted migration, rejected attempts and runtime restoration.

The [v0.2.21 migration](benchmarks/2026-11-26-v0221-integration.md) preserves all 26 frozen adopted-profile probes. Three-load medians are Chinese single/dual **121.00/184.53 tok/s**, code single/dual **168.90/245.50 tok/s**. Production and main are unchanged; the 0.2.20 lineage below describes the inherited optimizations.

Development records for the dual RX 7900 XTX rig. The goal in [DEVELOPMENT.md](DEVELOPMENT.md)
is to match the production baseline — `main`, v0.1.43-rdna3.1, engine 0.1.43 — in measured
service speed and correctness before merging. Experimental runs use an isolated maintenance
window and verify production restoration afterward.

The profile inherited from dev/0.2.20 is the template below: the earlier optimization set plus the gfx1100 exact
subset, `STRATA_EXPDEC=1` and the RDNA hyper-connection read (`STRATA_GR_RDNA=1`, with `STRATA_GR_FUSED` absent).
The [b477 adoption record](benchmarks/2026-11-24-v0220-grdna-adoption.md) holds the prior three-way numbers:
historical production 0.1.43, pristine v0.2.20 and this profile. Reuse the production and same-version stock
measurements unless their identities or the protocol change, and use paired runs for adoption decisions, not
cross-window rankings.

The [v0.2.20 campaign summary](benchmarks/2026-11-24-v0220-summary.md) lists every screen of this line with its
verdict, including the single-card stock comparison.

Adoption gates so far: the exact subset ([b464](benchmarks/2026-11-23-v0220-exact.md)), `STRATA_EXPDEC`
([b472](benchmarks/2026-11-23-v0220-expdec.md)) and the 9-worker option
([b475](benchmarks/2026-11-23-v0220-pool9.md)) were output-identical to the frozen
[0.1.65 reference](reference-0.1.65.json). The RDNA read changes the summation order and was adopted under a
declared quality review ([b476 diagnostic](benchmarks/2026-11-24-v0220-grdna-diagnostic.md),
[b477](benchmarks/2026-11-24-v0220-grdna-adoption.md)): the control still matches the reference, the candidate
differs only on the raw Chinese greedy probes, sampled and code probes and the objective suite agree, and the
scored texts show no degeneration. Measurements control expert residency with explicit phase-specific cache
budgets; the template leaves sizing to the engine.

## Rig and protocol

- 2x RX 7900 XTX (gfx1100), layer split 26/22, ROCm 10.1; Qwen3.8-Flash-Next GSQ-RCO IQ3_XXS
  weights, fixed expert-profile seed per arm.
- 1024 output tokens, 3 repeats, medians for confirmations; 768 tokens, one repeat for
  screening. Single lane = one request on both GPUs; dual lane = total tokens over the batch
  wall time including TTFT. Greedy, seed 42, presence penalty 1.5, caches disabled.
- Data per round: [benchmarks/](benchmarks/), run plans in [experiments/](experiments/),
  numerical probes and audits in [validation/](validation/). Reproduction entry points:
  [build.py](build.py), [bench.py](bench.py), [dual-xxs.template.json](dual-xxs.template.json).
  A day-level wrap-up in Chinese is in [benchmarks/2026-11-19-summary.md](benchmarks/2026-11-19-summary.md).

## Where the numbers stand (same-day, 2026-11-21)

| build | zh single | zh dual | code single | code dual |
|---|---:|---:|---:|---:|
| stock upstream v0.1.62 (same-day, 3 pairs) | 60.3 | 130.5 | 74.5 | 171.5 |
| **dev/0.2.16 adopted combo (same-day, 3 pairs)** | **124.6** | **158.0** | **161.9** | **216.7** |
| production legacy 0.1.43 (live probe, own config) | 139.5 | ~204† | 188.3 | ~269† |

† per-lane decode sums through the live config; not methodology-matched (see the
[same-day record](benchmarks/2026-11-21-legacy-dev-sameday.md)). Production is at its
historical speed — an earlier claim of degradation was a preflight-path error (the
campaign "official" arms had been measuring an upstream 0.1.62 package since 11-18).
Dev trails production by ~11–14% on single lanes; the remaining work is the documented
structural per-window gap.

## Adopted (template revision `shstream-aux-pool12-zero-pcie-exact-expdec-rdnagr`)

1. **Penalty-history pipelining** — `STRATA_HYBRID_PENALTIES=1` + `STRATA_PW_WITH_BATCH=1`
   (round 1; the largest single win over stock v0.2.16).
2. **Local BF16 hyper-connection read** — `STRATA_LAB_RDNA_GR=1`, split-K down and
   partial-folding up, gfx1100-gated (round 1).
3. **Shared expert on its own stream** — `STRATA_SH_STREAM=1` (rounds 15–17: zh single +3.9%,
   zh dual +6.9%, code single +4.5%).
4. **Auxiliary CPU placement** — `STRATA_AUX_CPUS=auto` (rounds 15–17).
5. **CPU expert pool widened** — `--pool-workers` 8 → 12 (rounds 21–22: zh single +5.0%,
   zh dual +3.4%, code flat).
6. **Fixed-zero expert PCIe graph** — `STRATA_LAB_RDNA_ZERO_PCIE_GRAPH=1` in the dual-gfx1100
   template with `--pcie-frac 0`. Three fresh-load pairs improved throughput medians by
   +3.7% / +7.1% / +4.6% / +1.2% in the same workload order. The engine-wide default stays
   off. See the [paired results and validation scope](benchmarks/2026-11-20-zero-pcie-graph.md).
7. **gfx1100 exact subset** - `STRATA_MMVF_ROWS=1`, `STRATA_ATTN_LANECELL=1`, `STRATA_ATTN_MERGE_V2=1`,
   `STRATA_SMALL_UNR=3`, with `STRATA_GFX11_DEFAULTS=0` kept (b464: Chinese dual +6.34% in three pairs,
   reference parity).
8. **Expert gate/up decode kernel** - `STRATA_EXPDEC=1` (b472: Chinese single +2.92%, dual +3.55%,
   reference parity).
9. **RDNA hyper-connection read** - `STRATA_GR_RDNA=1`. Never set `STRATA_GR_FUSED`, not even to 0: the
   upstream port allocates its fusion barrier whenever the variable is present, which aborts the first
   graph capture (b476). Adopted under the quality review after b477: +5.61% / +5.03% / +3.84% / +4.69% in
   the Chinese single/dual, code single/dual order, each faster in three pairs.

## Validated but not enabled by default

Each has numerical probes and measured records; none is a clear win on all four lanes of
this rig, so the default template stays unchanged: HC Q8/A8, exact-order MMVQ, Q5_K output
head, K-quant projection kernels (plus the per-format selector), the 32-column BF16 HC
quantize kernel (`STRATA_LAB_RDNA_GR_Q32`, round 24), the draft-layer HC fusion entry
(`STRATA_QFUSE_HC`, round 25), and the CPU expert-result combine path
(`STRATA_LAB_RDNA_COMBINE_CPU`, rounds 26–27: no overall gain — recovering one legacy path
is not the same as recovering the legacy engine's speed).

The [b475 screen](benchmarks/2026-11-23-v0220-pool9.md) compares 12 and 9 CPU pool workers on that profile:
9 workers keep exact reference parity and gain 2.69% on Chinese dual in all three pairs, at -1.73% on code
dual — an opt-in trade-off, not a uniform win.

## Kept negative results

- `--pool-workers 9` versus 12 on the adopted profile (RDNA read on): output-identical, Chinese single -2.67%; Chinese dual -3.65%; Code single +0.06%; Code dual -2.56% — 12 kept ([b478 record](benchmarks/2026-11-24-v0220-pool9-adapted-b478.md)).
- `--spec-min-p 0.35` versus 0.5 on the adopted profile: more drafts at a lower acceptance rate, Chinese single
  +1.96% and code single -2.51% — not adopted ([b481 record](benchmarks/2026-11-24-v0220-specminp-b481.md)).
- CPU governor `performance` versus `powersave` on the adopted profile: single lanes within 1.8%, dual lanes
  slightly slower — neutral, host left at `powersave` ([b480 record](benchmarks/2026-11-24-v0220-governor-b480.md)).
- `--pool-workers 16` versus 12 on the adopted profile (RDNA read on): output-identical, Chinese single -3.27%; Chinese dual -2.26%; Code single -1.33%; Code dual -0.89% — 12 kept ([b479 record](benchmarks/2026-11-24-v0220-pool16-adapted-b479.md)).
- `--pool-workers 9` versus 12 on the same profile: within 1.7% on code dual, +2.69% Chinese dual — adopted
  only as an opt-in choice ([2026-11-23 record](benchmarks/2026-11-23-v0220-pool9.md)).
- The gfx1100 parallel batch-slot stack (seven switches) on the exact + EXPDEC profile: output-identical, every
  lane within 1.3% — neutral, not adopted; KV streaming keeps one attention launch per row here
  ([2026-11-23 record](benchmarks/2026-11-23-v0220-parallel.md)).
- `STRATA_GDN_SPLIT=1` on the exact + EXPDEC profile: output-identical on the 26-probe reference, but
  Chinese dual -6.70%, code single -2.32%, code dual -2.43% in three pairs — rejected
  ([2026-11-23 record](benchmarks/2026-11-23-v0220-gdnsplit.md)).
- `STRATA_LOOKAHEAD=0` (routing prefetch worker off): Chinese dual +8.03% in every pair but
  code single −2.17% in every pair — not a clear all-lane win, so the look-ahead stays on
  ([2026-11-20 screen](benchmarks/2026-11-20-lookahead-off.md)).
- `STRATA_MTP_HIP_PRIO=1` (draft stream at highest priority): Chinese dual +3.83% in every
  pair, Chinese single −2.63% median, code flat — not adopted on gfx1100
  ([2026-11-20 screen](benchmarks/2026-11-20-mtp-prio.md)).
- `STRATA_LAB_RDNA_POOL_QUEUE=1` (async C through a single-pool SPSC runner): correctness
  bit-exact, throughput −4.8% to −10.4% — the synchronous inline serve model wins; every
  bounded serve-model restructure on this rig has now measured slower
  ([2026-11-21 screen](benchmarks/2026-11-21-pool-queue.md)).
- Legacy fused-A8 HC port: **rejected on measurement** — on the real production binary the
  fusion is worth ~0.3 ms/window on Chinese and nothing on code, below the grid-barrier
  port-risk threshold ([2026-11-21 record](benchmarks/2026-11-21-real-fused-ablation.md); the
  earlier "inconclusive" verdict had tested an upstream package by mistake).
- Local HC Q8 on the zero-PCIe baseline: **rejected** — after root-causing and fixing the
  tier-thread residency-upload deadlock it tripped (`res_put`, now stream-ordered), the
  three-pair confirmation measured −4.5%/−3.8%/−2.4%/+0.2%; the earlier one-load
  Chinese-dual gain did not reproduce
  ([2026-11-21 record](benchmarks/2026-11-21-hc-q8-resolved.md)).

- `STRATA_TSUM=1` aborts the engine at load on gfx1100 (SIGABRT, exit −6). Do not enable
  (round 21).
- CPU pinning (`--pool-cores`) and task-granularity changes (`--pool-tasks 13`): no overall
  gain; session drift between control loads reached 3.3% on zh single, which is why
  single-shot bests are never promoted (round 23).
- `STRATA_CPU_IQ_ALL_EXACT1`: parity-clean but 3–4% slower than `STRATA_IQ_MT_MIN=1` (round 14).
- `STRATA_ADAPT_OVERLAP` (the upstream counterpart of pipelined expert-residency refresh)
  excludes layer-split deployments by design (`generate.cpp:14311`).
- `--pool-workers 16`, `STRATA_COPY_HIP_PRIO`, `STRATA_COLD_OVERLAP=4`, `STRATA_LFUSE`,
  `STRATA_GU_SWIGLU`, draft/queue variants: screened, not adopted.
- 18 environment names inherited from the 0.1.43 production config are inert on engine
  0.1.62 — [validation/2026-11-19-switch-audit.md](validation/2026-11-19-switch-audit.md).

## Correctness notes

- Frozen-condition text checks (serial repeat / pipelined / forced rollback) passed in
  rounds 24 and 27, and failed during one afternoon window even for control configurations
  (rounds 18–20). The root cause of that transient is unknown; passing again does not count
  as a fix, and the check is never used to cherry-pick results.
- The three-question arithmetic smoke test still fails the order-JSON item on every build
  tested, including the production baseline's lineage. No page-quality claims are made.

## Where the remaining gap is (round 13, frozen per-stage profiling)

+1.93 ms/window over the rebuilt baseline on the GDN main stage (zh): ~45% wait chain
(waitB + PCIe group + waitCPU), ~18% hyper-connection read residual, ~24% spread across
compute kernels; copy+combine is faster on dev. Instrumentation disables fused paths, so
these are attribution numbers, not service speed. Next major items named in the day summary:
route publishing, CPU-result publish waits and the HC residual, measured with observation
methods that keep the normal fused paths on.

## Provenance

Source lineage, binary hashes and the blob-level audit of the legacy baseline:
[PROVENANCE.md](PROVENANCE.md). `main` did not change today and remains the serving
baseline; the merge gate in DEVELOPMENT.md is not met yet.
