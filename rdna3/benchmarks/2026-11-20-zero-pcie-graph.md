# Fixed-zero expert PCIe verifier graph

**Decision: enable in the fixed-zero dual-gfx1100 development template only.** Three fresh-load pairs improved all four throughput medians, and the frozen token and rollback checks passed. The engine-wide default and production configuration are unchanged.

## Why test this

With `--pcie-frac 0`, legacy 0.1.43 omitted the PCIe expert branch from the verifier graph. The corresponding development graph still recorded the flag-B wait, staging kernels and empty grouped expert launches. This is separate from the activation hand-off between GPU stages. Earlier 26/22 layer captures suggest six removable nodes per layer and group in kernel-staging mode (156/132 for a one-group window). That is a prediction from the historical graph, not a measurement of the current graph or of throughput.

R29 measured single requests with both engines explicitly using two pipeline windows; it was not a serial-versus-pipelined comparison. Chinese window wall time was 14.06 → 17.07 ms with 395 → 385 windows, and code was 13.41 → 16.18 ms with 304 → 321 windows. These historical runs used active expert adaptation. The host GPU-reach counter includes GPU progress and host polling; it does not isolate CPU queueing. The earlier timestamp comparison also used different profiling modes. Neither comparison assigns the window gap to this branch.

## Scope of the change

`STRATA_LAB_RDNA_ZERO_PCIE_GRAPH=1` is a load-time opt-in for native HIP serving on an actual two-card gfx1100 layer split. It requires an explicit numeric `--pcie-frac 0` on both stages and excludes automatic or balanced share, dynamic fetch, device planning and remote/peer experts. Other configurations keep the full graph. The common verifier recorder applies the mode to both stages, solo parity verifiers and batch captures. Resident expert work, CPU-ready publication, shared-expert synchronization, activation hand-off, and host-side ordered B publication are unchanged.

In the armed mode, both solo and batch request parsing reject a malformed, non-finite or nonzero `pcie_frac` override before any prompt or slot admission. A nonempty PCIe plan is rejected before publishing A. The split dispatch releases GPU waits with the existing bounded cleanup path and exits on that invariant violation; a queued commit must not lead to emitted tokens or reuse of that session.

## Build and correctness

- Base: `0f692e18d83ee48a4334e6e4cc8741f8f23a6362` plus the isolated experiment patch. ROCm 10.1, HIP Release, `gfx1100`. Tested `strata` SHA-256: `714e1894789f367aa214fe81a678a479731a03acc8857ffe61d7f7bf58949e87`.
- The first maintenance attempt passed `hip_flag_order_test` on both GPUs but stopped before model load because system Python could not import `regex`. Production was restored. The corrected runner uses the bundled interpreter and encodes both diagnostic prompts before stopping production. The second maintenance attempt again passed the ordered-flag test on both cards.
- 28 captured graph comparisons covered both stages and both solo verifier parities (widths 1–4), plus batch graphs (widths 2–4). On the matched shapes, only the expected wait, staging and expert kernel counts changed. Each graph lost 156 nodes on stage 0 and 132 on stage 1. For example, the four-row solo graphs changed from 1,042/874 to 886/742 nodes; the four-row batch graphs changed from 1,226/1,050 to 1,070/918 nodes.
- Each frozen arm generated 96 tokens per prompt and mode for Chinese and code with adaptation and suffix drafting off. Token IDs matched between arms for serial, pipelined, forced-rollback and batch requests. The serial and both pipelined outputs matched within each arm; each arm recorded 83 rollbacks in total. This is an exact-output check on the stated inputs, not a broad quality evaluation.
- The armed process rejected 14 invalid `pcie_frac` overrides across `GEN` and `BGEN`, then accepted zero-share requests. A separate `--pcie-frac 0.75` load with the switch requested retained the full graph and logged 386 rings with nonempty PCIe plans.

## Service throughput

Two RX 7900 XTX cards, 26/22 layer split, GSQ-RCO IQ3_XXS, identical expert-profile seed, power caps and 12 pool workers. Expert adaptation remained on in the scored runs; prompt and conversation caches were off (`cache_n=0`). Greedy seed 42, presence penalty 1.5. The same binary was independently loaded six times, in off/on, on/off, off/on order; the second pair reversed workload order in both arms. All 36 scored requests reached 1,024 output tokens. Graph-node diagnostics were off during scoring.

| Workload | Off median (tok/s) | On median (tok/s) | Median change | Change in pairs 1 / 2 / 3 | Median TTFT off → on (ms) |
|---|---:|---:|---:|---:|---:|
| Chinese single | 118.90 | 123.30 | +3.70% | +3.70% / −0.76% / +13.87% | 307 → 292 |
| Chinese dual | 154.74 | 165.69 | +7.08% | +3.59% / +5.39% / +7.08% | 624 → 613 |
| Code single | 153.90 | 161.00 | +4.61% | +3.64% / +4.04% / +4.61% | 285 → 281 |
| Code dual | 213.60 | 216.23 | +1.23% | +4.33% / +5.04% / +1.23% | 576 → 549 |

Single-request throughput is engine decode on both GPUs; dual throughput is total output tokens over batch wall time, including first-token wait. Dual TTFT is the median of the per-burst two-request TTFT medians across the three loads. Median change uses the ratio of arm medians; it is not the median of the pairwise changes.

Chinese dual and both code workloads improved in all three pairs. Chinese single was noisy, including one −0.76% pair. The second code-dual pair was slower in both arms than the other pairs; it remains in the results. Output hashes differed even across fresh loads of the same arm under the adaptive workload, and draft counts varied. That does not establish an arithmetic regression, nor does the frozen check establish identical adaptive output. This campaign establishes service gains against the same development binary, not the remaining gap to production or the individual cost of a removed kernel.

Production 0.1.43 was verified loaded at the end of the 16 min 57 s campaign, with the original configuration hash, the test listener closed, no monitor faults and no change in the sampled PCIe AER counters. Per-load throughputs, TTFT, draft counts and hashes are in the [structured record](2026-11-20-zero-pcie-graph.json).
