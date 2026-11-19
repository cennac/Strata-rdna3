# rdna3 — dev/0.2.16 optimization campaign (2026-11-19)

This directory holds the development records for the `dev/0.2.16` branch (upstream v0.2.16,
engine 0.1.62) on the dual RX 7900 XTX rig. The goal is set in [DEVELOPMENT.md](DEVELOPMENT.md):
match the production baseline — `main`, v0.1.43-rdna3.1, engine 0.1.43 — in measured service
speed and correctness before anything merges. Production keeps serving from `main` throughout;
every campaign stops the test service, runs its arms interleaved against a same-build control,
then restores and audits production (service active, config hash unchanged, test port closed,
AER counters unchanged).

## Rig and protocol

- 2x RX 7900 XTX (gfx1100), layer split 26/22, ROCm 10.1; Qwen3.8-Flash-Next GSQ-RCO IQ3_XXS
  weights, fixed expert-profile seed per arm.
- OpenAI-style requests against a local test instance: 1024 output tokens, 3 repeats, medians;
  zh and code prompts, single-lane (one request on both GPUs) and dual-lane aggregate
  (total tokens / batch wall time, TTFT included). Greedy decoding, seed 42, presence penalty
  1.5, conversation and prompt caches disabled. Screening passes use 768 tokens, one repeat.
- Full data per round: [benchmarks/](benchmarks/) (markdown + JSON), run plans in
  [experiments/](experiments/), numerical probes and audits in [validation/](validation/).
  Reproduction entry points: [build.py](build.py), [bench.py](bench.py),
  [dual-xxs.template.json](dual-xxs.template.json).

## What is in the recommended dev combination today

1. **Penalty-history pipelining** (`STRATA_HYBRID_PENALTIES=1` + `STRATA_PW_WITH_BATCH=1`) —
   restores single-request pipelining under non-zero penalties (round 1; the single largest
   win over stock v0.2.16).
2. **BF16 RDNA3 hyper-connection read** (`STRATA_LAB_RDNA_GR=1`, kernel in
   `engine/src/kernels/cuda/rdna_gr_local.cu`) — round 1.
3. **Shared expert on its own stream** (`STRATA_SH_STREAM=1`) — rounds 15–17, 1024x3 medians
   vs interleaved control: zh single +3.9%, zh dual +6.9%, code single +4.5%.
4. **Auxiliary CPU placement** (`STRATA_AUX_CPUS=auto`) — rounds 15–17.
5. **CPU expert pool widened** (`--pool-workers` 8 → 12) — rounds 21–22: zh single +5.0%,
   zh dual +3.4%, code flat.

Template revision: `shstream-aux-pool12`. All of today's gains are configuration-level; the
engine binary under test is unchanged from round 12 (`fd933531…`).

## Where the numbers stand

Across the day's interleaved controls the three additions adopted on 2026-11-19 afternoon
(items 3–5) accumulate to zh single +6.9%, zh dual +5.5%, code single +4.4%, code dual +1.1%.
Current best medians are roughly 115.6 / 162.1 / 152.4 / 213.1 tok/s (zh single, zh dual,
code single, code dual); the production baseline measures 139.1 / 185.8 / 195.5 / 235.8, so
the remaining gap is about −17% / −13% / −22% / −10%. TTFT on dual lanes has been ahead of
the baseline since round 1.

## Where the remaining gap is (round 13, frozen per-stage profiling)

With adaptivity frozen and stage instrumentation enabled (diagnostic numbers, not service
speed), the dev engine spends +1.93 ms/window over the rebuilt baseline on the GDN main
stage (zh):

| bucket | delta ms/window | share |
|---|---:|---:|
| wait chain (waitB + PCIe group + waitCPU) | +0.83 | ~45% |
| hyper-connection read residual | +0.35 | ~18% |
| compute kernels (shared+quant, router, gemv) | +0.46 | ~24% |
| copy+combine (dev is faster) | −0.24 | — |

The wait chain is overlap scheduling of CPU experts and PCIe staging — the part of the legacy
serving layer (single-request pipeline, dual-request scheduling, expert-residency refresh,
session-copy staging) that upstream 0.1.62 implements natively but differently. That port is
the next major item and is deliberately out of scope for switch-level work.

## Kept negative results and caveats

- `STRATA_TSUM=1` aborts the engine at load on gfx1100 (SIGABRT, exit −6). Upstream enables
  it on gfx1151/gfx12; do not enable it here. Round 21.
- `STRATA_CPU_IQ_ALL_EXACT1` (upstream's generalized version of the exact-order single-token
  IQ3_S rows): parity-clean on this CPU (`iq_avx2_parity`, 0 failures) but 3–4% slower than
  `STRATA_IQ_MT_MIN=1`, which already routes single tokens through the fast AVX2 rows.
  Not adopted. Round 14.
- `STRATA_ADAPT_OVERLAP` (the upstream counterpart of pipelined expert-residency refresh) is
  excluded on layer-split deployments by design (`generate.cpp:14311`); the local dual-GPU
  split cannot use it.
- One content-level early stop (977/1024, finish reason `stop`) occurred with the shared-expert
  stream in round 16; a second one is on record from round 9. Round 17's confirmation run had
  twelve clean lanes. Under monitoring.
- The frozen serial-repeat text check became unpassable on this rig during the afternoon for
  the control configuration itself (four control groups, 6/6 zh runs differ between immediate
  repeats; the same protocol passed in the morning). Until that reproducibility is understood,
  the check is not used to discriminate switches (rounds 18–20; open issue).
- 18 environment names inherited from the 0.1.43 production config are inert on engine 0.1.62;
  the list and the effective-switch audit are in [validation/2026-11-19-switch-audit.md](validation/2026-11-19-switch-audit.md).
- `--pool-workers 16`, `STRATA_COPY_HIP_PRIO`, `STRATA_COLD_OVERLAP=4`, `STRATA_LFUSE`,
  `STRATA_GU_SWIGLU` were screened and not adopted (rounds 15, 21–22).

## Provenance

Source lineage, binary hashes and the blob-level audit of the legacy baseline are in
[PROVENANCE.md](PROVENANCE.md). Nothing in `main` changed today; `main` remains the serving
baseline and the merge gate in DEVELOPMENT.md is not met yet.
