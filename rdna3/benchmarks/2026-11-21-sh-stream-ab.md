# Shared-expert fork stream A/B on the fixed-zero template

**Decision: keep `STRATA_SH_STREAM=1` in the development template.** The predeclared rule was to remove the fork if the off arm won the screen; the opposite happened — the fork arm was faster on every workload median, so this screen instead confirms, with paired measurements on the current template, the adoption that had been carried unchanged since the r28 era.

## Why this was screened

The legacy-20261110 production runtime config sets no `STRATA_SH_STREAM`, and the HIP default in the engine is off (`sh_stream_on`, `engine/src/core/verify.cpp:361-371`). The development template has carried `=1` through every published arm since r28, so the per-layer fork (4 event/dependency nodes per layer across 48 layers) had never been paired against off on the fixed-zero template. The in-tree comment's own HIP evidence (R9700 62.9 vs 43.4 t/s; #816 42.5 → 56.4 t/s) is single-GPU and suggested the fork is a net loss there, making the template line a plausible share of the remaining dev-vs-legacy per-window gap.

## What ran

Same clean `9ba93ba` binary (`268e6cfb…`, zero-overlay build) in both arms; dual gfx1100, 26/22 split, fixed-zero PCIe graph, BF16 HC, caches off, matched profile seed, greedy with presence penalty 1.5; three interleaved fresh-load pairs (on/off, off/on, on/off, second pair workload-reversed), all 36 scored requests at 1,024 output tokens, diagnostics off. `on` = fork enabled (template), `off` = HIP default.

## Correctness gates (all passed)

- Frozen raw (14 requests × 96 tokens per arm): exact token IDs and `length` finishes identical across arms — serial repeat, pw2, forced-rollback, two-slot batch, no-penalty controls, explicit 64-token penalty history. Forced mode recorded 32 Chinese / 23 code rollbacks in each arm.
- Positive-temperature sampling (12 requests per arm): exact repeats and prefix continuation identical; continuations reused 126 Chinese / 116 code prefix tokens in both arms.
- Objective v7 (6 cases): no new failures; the same five cases failed in both arms (four output-cap truncations, the order JSON wrong in both).
- Graph histograms: 24 matched stage/parity/shape/width buckets with exactly the expected single difference — the fork arm runs one extra `native_quantize_q8_1_kernel` per layer (+26 front, +22 final; the fork disables the shared expert's reuse of the experts' q8_1 image, `qdedup` requires `!sh_fork`), every other kernel count identical, no event/dependency nodes in the dump.

The first b440 attempt was correctly aborted by its own zero-delta graph gate: the run-day logs showed the quantize delta above, which the pre-run assumption (fork changes no captured kernel node) had missed. The gate update encodes that measured transformation, and the r2 campaign re-ran all gates and pairs with the same binary and fresh loads.

## Paired service measurements on 66

| Workload | Median off → on (tok/s) | Change | Pair 1 / 2 / 3 | Median TTFT off → on (ms) |
|---|---:|---:|---:|---:|
| Chinese single | 119.20 → 123.70 | +3.78% | +3.78% / +6.45% / +6.99% | 298.3 → 291.0 |
| Chinese dual | 161.89 → 162.04 | +0.09% | +0.09% / −4.19% / +6.18% | 607.1 → 588.3 |
| Code single | 159.10 → 162.40 | +2.07% | +6.91% / +0.44% / +2.07% | 279.5 → 270.0 |
| Code dual | 213.29 → 220.52 | +3.39% | +1.35% / −0.40% / +4.26% | 555.3 → 541.3 |

Single-request rates are engine decode; dual rates are total tokens over batch wall time. Changes compare arm medians. Every workload median favors the fork, and all four TTFT medians dropped 2.4–3.4% with it. Under the predeclared rule (off must win ≥2% median in all three pairs) the fork is not removed — the template line stays.

## Reading

Two honest conclusions. First, the hypothesis is dead: the shared-expert fork is not a source of the remaining dev-vs-legacy window gap; production's fork-off default is not a speed advantage here. Second, the mechanism note — the fork arm pays one extra per-layer quantize launch (qdedup disabled with the fork) plus the event chain, and still wins every median with lower TTFT, so the cross-stream overlap of the shared expert with the doorbell/expert path beats both costs on this dual-card doorbell rig. The single-GPU HIP evidence in the engine comment does not transfer to this path; both sides of `sh_stream_on`'s comment are now measured on gfx1100 as well. Next candidates remain the merged gate/up mmvq pair and the QSA K/V pair identified in the node analysis.

Production 0.1.43 was restored and verified (same running executable hash, unchanged config, port 8101 closed, zero monitor faults). See the [structured record](2026-11-21-sh-stream-ab.json).
