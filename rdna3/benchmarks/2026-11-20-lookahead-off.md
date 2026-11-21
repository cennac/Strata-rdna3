# Routing look-ahead off screen

**Decision: keep the routing look-ahead enabled.** Turning `STRATA_LOOKAHEAD=0` improved Chinese dual in all three pairs but regressed code single in all three pairs, so it is not a clear all-lane win. The default and the dual-gfx1100 template are unchanged.

## Why this was tested

The scored logs of the fixed-zero PCIe campaign showed the routing-aware prefetch active (`routing-aware prefetch on`) while every decode request read `0.0 MB` of read-ahead and used `0.0 MB` of it. The decode gate's bookkeeping advanced over 30,000 window evaluations inside one load while the gate stayed closed. `RouterLookahead::submit` also copies up to 8 tokens of activations per dispatch call and wakes a worker that runs a BF16 router dot, beside the 12 pool workers. `STRATA_LOOKAHEAD=0` is the upstream A/B arm for it.

## Method

Same binary as the fixed-zero PCIe campaign (`714e1894…`, zero-PCIe graph on in both arms), same weights, profile seed, power caps, prompts, penalties and cache policy. Three fresh-load pairs (base, laoff / laoff, base / base, laoff; pair 2 reversed task order), 1,024 output tokens per request, Chinese and code at one and two simultaneous requests. Two additional frozen loads (adaptation and suffix drafting off) ran serial, repeated-serial, pipelined and forced-rollback requests at 384 tokens. All 36 scored requests reached full budget.

## Throughput

| Workload | Look-ahead on (median) | Off (median) | Median change | Change per pair |
|---|---:|---:|---:|---:|
| Chinese single | 123.60 | 125.90 | +1.86% | −3.40% / +0.40% / +4.57% |
| Chinese dual | 159.86 | 172.69 | +8.03% | +3.77% / +12.77% / +4.92% |
| Code single | 165.70 | 162.10 | −2.17% | −2.17% / −1.98% / −0.19% |
| Code dual | 222.32 | 220.99 | −0.60% | −1.14% / +3.47% / +0.32% |

In tok/s. Single-request figures are engine decode; dual figures are total tokens over batch wall time. Median change is the ratio of arm medians, not the median of pair changes.

## Frozen parity

For both Chinese and code, the serial repeat, pipelined and forced-rollback texts were byte-identical within each arm, and identical between the look-ahead on and off arms (SHA-256 `34e47874…` and `78b1040e…`). Page warming does not change arithmetic, and that held here.

## Frozen per-window measurement

The frozen loads are deterministic on this rig (identical window counts, rollback counts and total wall times across independent loads), which turns frozen-arm differences into exact measurements. Against the base arm, turning the look-ahead off saves **0.51 ± 0.06 ms per window on every frozen request** (Chinese 18.07→17.56 and 19.42→18.91; code 17.32→16.89 and 18.26→17.69) — the submit-path activation copies and the worker's router dot cost about half a millisecond per window with zero measured read-ahead benefit. The scored code-single regression (−2.17% in all three pairs) sits against this solo-window gain; with the scored arms' acceptance drift at ±1 ms/window the net effect stays unclear, which is a further reason not to flip the switch.

## Notes

- The first campaign attempt aborted on its first arm: the prepared control arm inherited `STRATA_LAB_RDNA_ZERO_PCIE_GRAPH='0'` from the previous campaign's off arm, so the engine did not enter the fixed-zero mode and the campaign's own assertion stopped it before any scoring. Production was restored automatically; the corrected run is the one recorded here.
- Production 0.1.43 was restored and verified loaded after both attempts; the configuration hash was unchanged, the test port was closed, and no monitor faults occurred.
- The consistent Chinese-dual gain and the consistent code-single loss are both real effects of this switch on this rig; neither is explained by the measured read-ahead (zero bytes used). Per-load numbers are in the [structured record](2026-11-20-lookahead-off.json).
