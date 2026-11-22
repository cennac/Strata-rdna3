# rdna3 — dev/0.2.18 optimization campaign (upstream v0.2.18, engine 0.1.64; records from 2026-11-19)

Development records for the `dev/0.2.18` branch (upstream v0.2.18, engine 0.1.64, merged on top of the validated `dev/0.2.17` line — upstream 0.2.18 is almost purely additive, adding the Tencent Hy4-Flash model family without touching the Qwen3.8 decode path) on the
dual RX 7900 XTX rig. The goal is set in [DEVELOPMENT.md](DEVELOPMENT.md): match the
production baseline — `main`, v0.1.43-rdna3.1, engine 0.1.43 — in measured service speed and
correctness before anything merges. Production keeps serving from `main` throughout; every
campaign runs its arms interleaved against a same-build control, then restores production and
audits it (service active, config hash unchanged, test port closed, AER counters unchanged).

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

## Adopted (template revision `shstream-aux-pool12-zero-pcie`)

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

## Validated but not enabled by default

Each has numerical probes and measured records; none is a clear win on all four lanes of
this rig, so the default template stays unchanged: HC Q8/A8, exact-order MMVQ, Q5_K output
head, K-quant projection kernels (plus the per-format selector), the 32-column BF16 HC
quantize kernel (`STRATA_LAB_RDNA_GR_Q32`, round 24), the draft-layer HC fusion entry
(`STRATA_QFUSE_HC`, round 25), and the CPU expert-result combine path
(`STRATA_LAB_RDNA_COMBINE_CPU`, rounds 26–27: no overall gain — recovering one legacy path
is not the same as recovering the legacy engine's speed).

## Kept negative results

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
