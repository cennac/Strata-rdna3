# Per-bucket profile A/B: production 0.1.43 vs dev 0.1.64 — the gap is the pool row-delivery chain, measured (b453)

**Outcome: diagnostic complete; per the predeclared rule the optimization campaign closes.** The refreshed per-bucket map of the remaining wall gap confirms round 33 on today's line — compute kernels at parity or dev-better — and pins the one dominant net-positive delta: the CPU-expert row delivery (waitCPU), ~+1.5 ms/window on stage 1 and +1.0 on stage 0, offset by dev's cheaper copy+combine and rebucketed router work. No new ≥0.5 ms regression bucket outside that known chain appeared.

## What ran

Three arms on the identical test config, each a fresh load with three solo 1,024-token requests (zh, code, zh-repeat), stamps on solo windows only:

- **leg-prof** — the production binary (`58854970…`, engine 0.1.43) under strict `STRATA_VERIFY_PROFILE` (both engines disable the same fused-path set → bucket-by-bucket comparable).
- **dev-prof** — the dev binary (`88c08615…`, engine 0.1.64) under the same strict mode.
- **dev-light** — the dev binary with `STRATA_VERIFY_PROFILE_LIGHT` (fused paths on).

Both engines print the stage tables natively in serve mode; the parser keys buckets by name (0.1.43's 33-bucket table vs 0.1.64's 34 with `router (proj+topk)` split out). No quality gates — cross-engine outputs differ by design.

## The paired walls and the map

| arm | zh | code | zh-repeat |
|---|---:|---:|---:|
| leg strict (ms/window) | 18.63 | 16.82 | 18.67 |
| dev strict | 21.57 | 20.26 | 21.72 |
| dev light (fused on) | 21.03 | 19.99 | 21.20 |

Paired strict delta: **+2.94 ms/window (zh), +3.44 (code)**. The net bucketed delta (+3.43/request) reconciles it. Where it lives (per window, strict arms):

| bucket (stage, segment) | leg | dev | Δ |
|---|---:|---:|---:|
| **waitCPU (s1 gdn)** | 0.22 | 1.70 | **+1.48** |
| copy+combine (s1 gdn) | 1.54 | 0.28 | −1.26 |
| waitCPU (s0 gdn) | 0.07 | 1.04 | **+0.97** |
| router+plan+bell / plan+bell+router (proj+topk) | 0.46–0.52 | rebucketed | ≈0 net |
| copy+combine (s0 gdn) | 0.82 | 0.33 | −0.49 |
| waitCPU (qsa, both stages) | 0.03–0.06 | 0.45–0.67 | +0.45…+0.64 |
| everything else | — | — | parity or dev-better |

Reading: legacy computes CPU-expert rows **early and copies/combines them inline** (its copy+combine bucket is 5× dev's), while dev defers rows to the pool and the window **waits** for their delivery — waitCPU ~+2.5 ms/request net of the offsets. This is the wait chain round 33 attributed at +0.82; under today's strict accounting it is larger, and it is the same slice the five bounded repairs (pump-age, serve-quantum, B-direct, dual pools, async pool queue) each attacked and each measured **negative on throughput** — shrinking the wait does not shrink the pipelined wall, because the chain overlaps the other card's GPU work in service mode. The one unbuilt idea (cross-window pool look-ahead) keeps its eroded premise: the delivery wait is real pool work, not idle-worker slack.

Also measured: on 0.1.64 the fused paths are worth ~−0.5 ms/window (dev light 21.03 vs dev strict 21.57); 0.1.43 has no light mode, so its fused truth stays the b436 number (fused −0.16 vs split). Compute kernels (gemv/conv/z/rec/out-proj/attention/heads) show no regression — the 0.1.64 window graph is not kernel-slower, it is scheduled differently.

## Disposition

- No engine or template change; production 0.1.43 restored and verified after every attempt (running hash `58854970…`, config unchanged, port 8101 closed).
- Attempts 1–4 aborted fail-closed on harness gates (0.1.43 prints no io-prefetch line and no verifier node listings under `STRATA_VERIFY_NODES` — only MTP graph listings; a missing `--batch 2`; an over-strict window-count check). Attempt 5 completed all three arms; the diff was computed from its results. Harness bundle `E:\AIPorject\66\artifacts\strata-profile-ab-20261122-b453` (offline tests 11/11 replaying real b436/r33/b452 lines).
- Structured record: [2026-11-22-profile-ab.json](2026-11-22-profile-ab.json); sanitized diff `profile-diff.json` in the bundle.

## Campaign state

Every lever is now measured-closed with a complete evidence chain: environment switches, placement (three configurations), stream priority (three levels), pool/serve restructures (five variants), capacity/cache knobs — and this round's kernel-level map shows no hidden regression to recover. The optimization line stands at ~2× stock single-lane decode, TTFT below production, quality equivalent, trailing production 0.1.43 by ~10–17% for a structural reason (pipelined row delivery vs legacy's serial inline path) that only a cross-window look-ahead redesign could touch — measured-negative neighbors, eroded premise. Closing the campaign here; a new round opens on an upstream version, model, or hardware change.
