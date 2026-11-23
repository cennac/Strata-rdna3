# RDNA hyper-connection read adopted under quality review (b477)

The development profile now includes the RDNA hyper-connection read: `STRATA_GR_RDNA=1` with `STRATA_GR_FUSED`
absent from the environment. b477 is the adoption window, run on dual RX 7900 XTX (26/22 split, ROCm 10.1,
Qwen3.8-Flash-Next GSQ-RCO IQ3_XXS) with the same integrated 0.1.66 binary from `53b8629`, expert-profile seed and
cache budgets in both arms; the [b476 diagnostic](2026-11-24-v0220-grdna-diagnostic.md) explains why the port had
failed here and records the first measurements.

## Gate

The kernel changes the summation order, so the frozen-token rule was replaced for this one change by a review
declared before the window: the control must still match the frozen [0.1.65 reference](../reference-0.1.65.json);
the candidate may differ only on the raw Chinese greedy probes; the sampled and code probes and the objective suite
must agree; and the scored texts must show no degeneration against the control (4-gram diversity, tail diversity,
line repetition, control characters, numbered items). All checks passed: 7 raw Chinese probes differ (first
difference at zero-based token 7 or 21), forced rollbacks are 33/23 against the control's 35/21, and cases 19 and
23 are the objective successes in both arms. The candidate's graphs carry `rdna_gr_down` and `rdna_gr_up` on both
stages.

## Paired result

Three alternating fresh-load pairs; pair 2 reversed the workload order. All 36 scored requests finished at 1,024
output tokens with caches off. Every workload was faster in all three pairs.

| Workload | Control tok/s | RDNA GR tok/s | Throughput | TTFT | Faster pairs |
|---|---:|---:|---:|---:|---:|
| Chinese single | 121.20 | 128.00 | +5.61% | -6.14% | 3/3 |
| Chinese dual | 169.01 | 177.51 | +5.03% | -6.56% | 3/3 |
| Code single | 158.70 | 164.80 | +3.84% | -4.45% | 3/3 |
| Code dual | 228.73 | 239.46 | +4.69% | -5.65% | 3/3 |

## Three-way comparison

Median **tok/s / TTFT in ms**. Production and stock are reused from their own campaigns.

| Workload | Historical production 0.1.43 | v0.2.20 stock | Adopted profile | vs stock | vs production |
|---|---:|---:|---:|---:|---:|
| Chinese single | 139.10 / 253.8 | 64.50 / 779.3 | 128.00 / 272.9 | +98.4% | -8.0% |
| Chinese dual | 185.75 / 960.8 | 112.69 / 1581.1 | 177.51 / 552.2 | +57.5% | -4.4% |
| Code single | 195.50 / 234.1 | 79.75 / 707.5 | 164.80 / 258.8 | +106.6% | -15.7% |
| Code dual | 235.83 / 873.2 | 106.99 / 1434.2 | 239.46 / 508.4 | +123.8% | +1.5% |

## Notes

Measurements fix expert residency with explicit phase-specific cache budgets; the template leaves sizing to the
engine. `STRATA_GFX11_DEFAULTS` stays 0 and the earlier pins are unchanged. `STRATA_GR_FUSED=1` (one-launch fused
read) still needs a source change to allocate its barrier outside graph capture and was not tested. Production
restoration was verified after the window; no deployment or `main` change. Per-pair values and gates:
[JSON](2026-11-24-v0220-grdna-adoption.json).
