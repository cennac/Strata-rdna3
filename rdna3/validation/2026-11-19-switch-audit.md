# 开关审计(2026-11-19,engine 0.1.62 / dev c5921db)

对照 `dual-xxs.template.json` 的67个 `STRATA_*`/`GPU_*` env 与 dev 引擎源码的全部 `getenv("STRATA_*")`(814个)。

## 上游已收编的本地工作树优化

- `STRATA_IQ3S_EXACT1`(本地工作树 `cennac-rdna3-dev` commit `fa6d445`,0.1.43基底)→ 上游0.1.62已泛化为 `STRATA_CPU_IQ_ALL_EXACT1`(native_expert.cpp:487-499 → `iq256_gu_rows_exact_one()`,覆盖16/17/18/21/22五格式,ggml逐位一致,默认关)。其速度收益在当前组合中已由 `STRATA_IQ_MT_MIN=1` 兑现(两者用同一快速解码,仅最终求和次序不同);第十四轮实测确认不可叠加且破坏窗口不变性,不采纳。
- `STRATA_PIPED_ADAPT_EVERY`(本地工作树 `cennac-rdna3-dev` commit `d2e955c`)→ 上游等价物 `STRATA_ADAPT_OVERLAP`(generate.cpp:2195)在 layer-split 部署上被上游明确排除(generate.cpp:14311 `"not on a layer split"`),本机双卡 `--layer-split 26 22` 无法启用;0.1.62流水线重构后有界嫁接风险高,评估后不移植。

## 模板中的无效残留名(engine 0.1.62 源码无 `getenv` 读取;在 0.1.43 现役上有效)

`STRATA_COMBINE_CPU=1`、`STRATA_CONVERSATION_PINNED_COPY=1`、`STRATA_GDN_CHUNK_GDV=0`、`STRATA_GR_RDNA=1`、`STRATA_GR_RDNA_Q8=1`、`STRATA_HYBRID_WINDOWS=1`、`STRATA_PA_KLDS=1`、`STRATA_PEER_TRANSPORT=host`、`STRATA_PF_GROUP_RES=1`、`STRATA_PIPED_ADAPT_EVERY=0`、`STRATA_DP_PREFILL=0`、`STRATA_PREFILL_MMQ_TILE_ROWS=32`、`STRATA_PREFILL_SMALL_MAX=4096`、`STRATA_PREFILL_SMALL_RING=96`、`STRATA_RDNA_MMVQ_WAVE=3`、`STRATA_RDNA_Q6K=1`、`STRATA_ROUTE_PUBLISH=1`、`STRATA_WAIT_FUSE=1`

注:`GPU_MAX_HW_QUEUES` 在 dev 有效(generate.cpp:3412,队列prime门控)。模板继承自现役生产配置,这些名字在 dev 上无害但易误导;因模板仅在确认全面更优组合时才改动,暂保留原样,以本文件为准。

完整 env 清单:`E:\AIPorject\66\artifacts\strata-optimize-0216b-20261119\evidence\dev-getenv-strata.txt`。
