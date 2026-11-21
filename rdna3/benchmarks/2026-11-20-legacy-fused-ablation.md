# Legacy fused-vs-split A8 ablation attempt (2026-11-20)

**Outcome: inconclusive as a service measurement; the HC fused-A8 port is deprioritized.** The production 0.1.43 binary does not reproduce texts across identical frozen requests on this rig, so cross-arm wall times compare different work. The pre-existing evidence (an IQ3_S service ablation at ≈1%, R33's cross-mode +0.38 ms/window) bounds the upside well below the pool serve model's 2.4 ms/window wait chain, which stays the primary target.

## What ran

Two loads of the production binary (`bdec5453…`), identical frozen configuration (adapt 0, swaps 0, caches off, same profile seed, pw2, pcie 0), differing only in `STRATA_GR_FUSED=1` (one-launch A8, the production setting) vs `STRATA_GR_FUSED=0` (two-launch split A8; main's selector is `v[0]=='1'`, so the off arm is unambiguous). Seven requests per arm (128 warm-up, zh/code 384 twice, zh/code 1024). Production was stopped for 6 min 7 s and restored; 0.1.43 verified loaded after.

## Why it is inconclusive

Every request's text hash differs across repeats and arms — inside the same arm too (fused zh-384 repeats `0d63fef2…` vs `c1416c2e…`; split code-384 repeats `1bf5120…` vs `d0ac68f…`). Only two same-text repeat pairs exist (fused code-384, split zh-384), and their walls are tight (±0.1–0.7%), which shows the jitter is workload drift, not noise. With different continuations per request, wall-time deltas between arms measure text luck, not the HC launch count.

## The finding that matters

**Engine 0.1.43 is text-nondeterministic under this frozen greedy+presence-penalty protocol on this box, while the development engine (0.1.62 with the penalty-history pipeline) reproduced every frozen request byte-identically across four loads today.** This reverses the R14–R20 situation, when frozen serial repeats failed on this machine for development-side control configs and the cause was left open. Today's evidence puts the instability on the legacy side of the pair under the current conditions; it does not locate its cause (penalty-history handling and timing-dependent gating both remain candidates). Structured per-request data: [record](2026-11-20-legacy-fused-ablation.json).

## Decision

The HC fused-A8 port is shelved: the measured ceiling (~0.4–1 ms/window) is small against its port risk (the grid barrier needs workgroup co-residency proof in dev's overlapping graphs), and today's service route cannot refine it. Revisit only with a fixed-work probe if the pool serve work lands and a per-window HC gap remains. The next primary target is the pool serve model (stage-0 waitA ~1.0 ms + stage-1 waitCPU ~1.7 ms per the [light profile](2026-11-20-light-profile.md)).
> **Superseded (2026-11-21):** this attempt's preflight engine path pointed at an upstream 0.1.62 package with no legacy HC kernels, so `STRATA_GR_FUSED` never engaged. The ablation was redone on the real production binary and the port was rejected on measurement: [real-binary record](2026-11-21-real-fused-ablation.md).
