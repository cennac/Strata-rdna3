# RDNA hyper-connection read on 0.1.66: capture abort explained, large gains, token parity not held

Diagnostic record, not adopted. b476 paired the [b472 profile](2026-11-23-v0220-expdec.md) against the same
profile with `STRATA_GR_RDNA=1` and `STRATA_GR_FUSED` removed from the environment, on dual RX 7900 XTX
(26/22 split, ROCm 10.1, Qwen3.8-Flash-Next GSQ-RCO IQ3_XXS). Same integrated 0.1.66 binary from `53b8629`,
same expert-profile seed and cache budgets.

## Why the port had failed here

The earlier attempts all kept `STRATA_GR_FUSED=0` in the environment. The port allocates its
persistent-fusion barrier whenever that variable is present, regardless of value, and the allocation lands
inside the first verifier graph capture, which invalidates the capture. The non-fused path never uses the
barrier. With the variable absent the upstream kernels `rdna_gr_down` and `rdna_gr_up` load and are captured
on both stages (416 nodes on stage 0, 360 on stage 1). The development profile had been running without any
RDNA hyper-connection read since the v0.2.19 merge; production 0.1.43 has the fork's original kernels.

## Quality, recorded not gated

The kernel uses another summation order. Against the frozen [0.1.65 reference](../reference-0.1.65.json):
7 of the 14 raw probes differ, all Chinese greedy (first differing token at zero-based index 7 or 21), and
forced rollbacks moved from 35/21 to 33/23. All 12 sampled probes and all 6 code raw probes matched exactly.
The objective suite gave identical results in both arms (cases 19 and 23 correct, no new failures). The
1,024-token scored texts showed the same 4-gram diversity as the control and no control characters.

## Speed, diagnostic

One candidate Chinese dual burst in pair 3 stopped naturally at 806 tokens, so the window has no eligible
aggregate score. Medians over complete pairs:

| Workload | Control tok/s | RDNA GR tok/s | Throughput | TTFT | Complete pairs |
|---|---:|---:|---:|---:|---:|
| Chinese single | 117.00 | 125.60 | +7.35% | -6.70% | 3/3 |
| Chinese dual | 174.53 | 178.86 | +2.48% | -6.17% | 2/3 |
| Code single | 158.90 | 168.40 | +5.98% | -5.29% | 3/3 |
| Code dual | 228.96 | 241.55 | +5.50% | -6.70% | 3/3 |

## Three-way context

Median **tok/s / TTFT in ms**. Production and stock are reused from their own campaigns; the last column is
this window's diagnostic arm.

| Workload | Historical production 0.1.43 | v0.2.20 stock | b472 profile + RDNA GR | vs stock | vs production |
|---|---:|---:|---:|---:|---:|
| Chinese single | 139.10 / 253.8 | 64.50 / 779.3 | 125.60 / 272.9 | +94.7% | -9.7% |
| Chinese dual | 185.75 / 960.8 | 112.69 / 1581.1 | 178.86 / 555.4 | +58.7% | -3.7% |
| Code single | 195.50 / 234.1 | 79.75 / 707.5 | 168.40 / 258.9 | +111.2% | -13.9% |
| Code dual | 235.83 / 873.2 | 106.99 / 1434.2 | 241.55 / 504.5 | +125.8% | +2.4% |

## Status

Nothing here is adoptable under the token-parity rule: the candidate fails the frozen reference on Chinese
greedy probes. Adoption would need a separate decision to accept this summation-order change after a quality
review (objective suite, degeneration checks on real prompts, within-arm repeatability) and a clean three-pair
window. Production restoration was verified; no deployment or `main` change. Per-pair values:
[JSON](2026-11-24-v0220-grdna-diagnostic.json).
