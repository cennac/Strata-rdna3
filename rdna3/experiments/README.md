# 0.2.16 RDNA3 实验开关

这里保存筛选计划，包括没有获得整机收益的组合。默认部署模板不因单次微基准变快而改变。先按仓库 README 构建，使用相同模型、专家画像、提示、惩罚、缓存、输出长度及功耗做开／关对照。

| 开关 | 条件与行为 | 验证范围 |
|---|---|---|
| `STRATA_LAB_RDNA_GR=1` | 选择迁移的 BF16 HC read | 两张 gfx1100，T=1…8，pending write、inject 与图重放 |
| `STRATA_LAB_RDNA_HC_Q8=1` | 同时设置 `STRATA_HC_Q8=1`、`STRATA_LAB_RDNA_GR=1`，读取上游加载器已有的 Q8_0 副本 | 对照上游 Q8 GPU 结果；未定义的 `lo` 工作区不参与比较 |
| `STRATA_GR_RDNA_A8=1` | 在上述 Q8 路径中量化激活，属于近似计算；inject 仍为 BF16 | 对照独立 CPU 标量 Q8/A8 参考；分别检查 `STRATA_GR_INJ_UP=0/1` |
| `STRATA_LAB_RDNA_MMVQ=1` | 迁移旧版稠密矩阵 wave32 布局，保持上游四组部分和的累加顺序 | Q4_0、Q5_0、Q8_0、IQ4_NL，各 1…16 列、边界形状与图重放，逐位比较 |
| `STRATA_LAB_RDNA_Q5_HEAD=1` | 将同一精确求和布局应用于2560输入、至少65536输出行的Q5_K输出头，限2…8列 | 逐位比较完整248320词表头、65537奇数行与回退形状；1列保留上游短行归约 |
| `STRATA_LAB_RDNA_KQUANTS=1` | 迁入main的gfx1100 IQ4_XS、Q6_K、IQ3_S专用核；Q4_K/Q5_K默认仅4…8列；1…8列以外回退。改变求和顺序，默认关闭 | `rdna_kquant_probe`检查上游误差、多列一致性、成对发射和图重放；不是逐位等同上游 |
| `STRATA_LAB_RDNA_KQUANT_TYPES=14,23` | 依赖上一项，按GGML类型选择内部投影核；12=Q4_K、13=Q5_K、14=Q6_K、21=IQ3_S、23=IQ4_XS。未设置时使用全部支持格式；空值、未知格式或格式错误时全部回退 | 掩码由进程首次读取后固定；未选格式与上游逐位一致，选中格式执行原容差检查 |

MMVQ 初次迁移仅用于 `g_multi_exact` 开启、`STRATA_TSUM` 路径关闭、输入维度2048…2560、输出行数至少1024、列数1…8的 small-format 调度。其余形状保留上游路径。第十轮另外增加K-quant专用核实验，没有更改MoE专家矩阵的调度。

Q5_K输出头是单独的实验开关，不依赖small-format开关。它同样要求 `g_multi_exact` 且关闭 `STRATA_TSUM`。该路径复用已有量化权重，没有额外的头权重副本；保持上游各部分和的累计次序，不采用旧Q5_K专用实现的另一种归约顺序。

HC Q8/A8 复用上游 Q8_0 权重，不分配第二份旧版平面 R8 权重。34字节量化块的码区只有2字节对齐，读取使用16位访问。没有启用旧版跨工作组自旋屏障融合。

## Fixed-zero expert PCIe graph

Set `STRATA_LAB_RDNA_ZERO_PCIE_GRAPH=1` before loading a native HIP service on a physical two-card gfx1100 split with explicit `--pcie-frac 0`. Automatic share, balance, dynamic fetch, device plans and remote/peer experts are outside the experiment. Ineligible loads keep the full graph. An armed load accepts only zero-valued `pcie_frac` request overrides. This does not remove activation transfers between the two cards.

Use `STRATA_VERIFY_NODES=1` in a separate diagnostic run to compare window and batch graph histograms by device, layer range and verifier; the verifier identity distinguishes parity graphs within a process. Leave diagnostics off during throughput tests. Run `cmake --build build --target hip_flag_order_test` and execute `build/hip_flag_order_test` with the matching ROCm runtime on idle GPUs. The [paired results](../benchmarks/2026-11-20-zero-pcie-graph.md) support enabling this only in the fixed-zero dual-card development template. Unset the switch if the service needs request-level nonzero shares.

## Pinned penalty-history staging

`STRATA_LAB_RDNA_PIN_PENALTY_HIST=1` stages the solo pw2 penalty-history upload through one page-locked buffer and drops the full-stream synchronize that followed it (the sync existed to protect the pageable staging vector's lifetime; it also drained the previous window's commit and sampling before the next window could be enqueued). The copy, window graph and sampling stay ordered on the shared last-stage stream; an event guards the buffer's rewrite; allocation failure falls back to the exact pageable path.

Every frozen-output gate passed with the switch on (exact token IDs across serial/pw2/forced/batch/no-penalty, sampled repeats and prefix continuation, identical graph histograms). The service pairs did not: Chinese single and dual lost throughput in all three pairs (−5.81% / −4.68% on arm medians), so the switch is **rejected** — the synchronize was pacing the last stage usefully, and host-side blocking at this upload is not where the remaining window gap lives. See the [paired record](../benchmarks/2026-11-21-pin-penalty-hist.md) before reconsidering host-unblocking changes on the pipelined loop.

## Routing look-ahead off

`STRATA_LOOKAHEAD=0` disables the routing-aware file-tier prefetch worker (`RouterLookahead`), the documented upstream A/B arm. On this rig its read-ahead measures zero used bytes while the gate bookkeeping keeps running, so it was screened on the fixed-zero template. The result was mixed (Chinese dual up in every pair, code single down in every pair) and the switch was **not** adopted; see the [paired record](../benchmarks/2026-11-20-lookahead-off.md) before touching it.

## File-tier prefetch off

On the tested mapped `experts.bin` pack, `STRATA_IO_PREFETCH=0` disables both file-tier prefetch and routing look-ahead; it is not an I/O-only counterpart to `STRATA_LOOKAHEAD=0`. The [paired test](../benchmarks/2026-11-21-file-tier-prefetch-off.md) found a code-dual gain but slower single-request medians. Keep the default for the general-purpose dual-card template.

## MTP draft stream priority

`STRATA_MTP_HIP_PRIO=1` raises the drafter's stream to the highest priority (own hardware queue per ROCm's per-level allocation; default-on for gfx12, opt-in here). gfx1100 exposes a real range (1..-1), and when the switch is set the engine logs `strata mtp: draft stream priority raised (range …)`. Screened on the fixed-zero template: mixed result, **not** adopted; see the [paired record](../benchmarks/2026-11-20-mtp-prio.md).

## 运行数值探针

```sh
cmake --build build --target rdna_hc_probe rdna_mmvq_probe rdna_kquant_probe -j 4
export LD_LIBRARY_PATH=/opt/rocm/lib:/opt/rocm/lib/llvm/lib
for gpu in 0 1; do
  build/rdna_hc_probe "$gpu" bf16
  build/rdna_hc_probe "$gpu" q8
  STRATA_GR_INJ_UP=0 build/rdna_hc_probe "$gpu" a8
  STRATA_GR_INJ_UP=1 build/rdna_hc_probe "$gpu" a8
  build/rdna_mmvq_probe "$gpu"
  build/rdna_kquant_probe "$gpu"
done
```

这些探针用于核对计算与图执行，不能替代回答质量测试。微基准是热权重条件下的局部耗时，不是服务吞吐。实验对象为 gfx1100；未据此推断其他 GPU 的收益。

K-quant探针使用固定种子生成的合法量化块及实际投影维度；预先固定容差为相对L2不超过`2e-5`，最大绝对误差不超过`1e-5 + 2e-5 * max(abs(reference))`。单列／多列检查用于IQ4_XS、Q6_K和IQ3_S；Q4_K/Q5_K在1…3列走回退，因此不要求它们与另一种布局逐位相同。成对发射只用于行数为8倍数且两矩阵同格式的情况。图重放必须逐位匹配该优化核直接发射的结果。

分格式选择针对同一个GSQ-RCO IQ3_XXS模型内部的混合量化矩阵，不是更换模型为IQ4或Q6。每个线程／设备／格式首次选中时输出一条诊断日志；图捕获也会记录，因此不能将日志条数当作实际执行次数。选择示例与独立掩码校验：

```sh
# 只启用内部Q6_K和IQ4_XS投影；服务加载前设置。
export STRATA_LAB_RDNA_KQUANTS=1
export STRATA_LAB_RDNA_KQUANT_TYPES=14,23
# 探针的第二参数是独立给定的期望位掩码，8404992 = (1<<14) | (1<<23)。
build/rdna_kquant_probe 0 8404992
build/rdna_kquant_probe 1 8404992
# 无效列表应完全回退；期望掩码为0。
STRATA_LAB_RDNA_KQUANT_TYPES='23,garbage' build/rdna_kquant_probe 0 0
```

探针默认不计微基准时间；仅需局部诊断时设置`STRATA_RDNA_PROBE_BENCH=1`。顺序计时的热状态和频率可导致同一路径也出现差异，不能替代服务对照。

筛选使用768输出、每项1次，适合排除退化组合。采用候选前需要1024输出、至少3次的独立加载复测，并继续核对惩罚历史和回滚行为。单路是一个请求使用两张GPU；双路吞吐使用总输出token除以从请求发出到整批结束的时间，包含首字等待。

## BF16 HC 32列与本地量化实验

`STRATA_LAB_RDNA_GR_Q32=1` 依赖 `STRATA_LAB_RDNA_GR=1`，默认关闭，仅改变本地BF16 HC读取的up阶段。工作组从16列／256线程改为32列／512线程，每组独占一个q8_1块，可在调用方允许时同时产生量化图像，省去独立量化发射。逐行求和顺序保持不变；Q8/A8权重路径不变。没有跨工作组计数或自旋屏障。

关闭 `STRATA_QFUSE_HC_LOCAL` 且未提供计数器时，仍须返回“未融合量化”，交给调用方执行独立量化。新增探针经实际调度入口检查这个回退，避免将未写入图像误报为已生成。

新版草稿层的 `mtp_qfuse_hc_on()` 只读取 `STRATA_QFUSE_HC`，不读取 `STRATA_QFUSE`，gfx1100默认关闭。要测草稿层的无计数器融合，另设 `STRATA_QFUSE_HC=1`；只开32列开关并不能证明草稿层已产生融合量化图像。首次选中日志标明T与图像指针是否存在，图捕获也会记录，同一T后续调用不重复输出，不能据第一条日志推断所有层的路径或执行次数。

```sh
cmake --build build --target strata rdna_hc_probe -j 4
export LD_LIBRARY_PATH=/opt/rocm/lib:/opt/rocm/lib/llvm/lib
for gpu in 0 1; do
  build/rdna_hc_probe "$gpu" bf16
  build/rdna_hc_probe "$gpu" q32plain
  build/rdna_hc_probe "$gpu" q32
  STRATA_RDNA_PROBE_ZERO=1 build/rdna_hc_probe "$gpu" q32
  STRATA_QFUSE_HC_LOCAL=0 build/rdna_hc_probe "$gpu" q32
done
```

`q32plain`／`q32` 对T=1…8、apply与inject开关检查五个浮点输出与旧16列核逐位一致；`q32`另将图像字节与旧核后单独运行的原生q8_1量化逐位比较，并检查缓冲尾部哨兵及图重放。零输入检查量化零值边界。微基准两臂包含的量化工作可能不同，不能从其顺序计时推算整机收益。

## CPU mapped-row combine 实验

开发版配置继承的 `STRATA_COMBINE_CPU=1` 没有对应实现，不会打开现役旧版的CPU结果合并优化。新增独立、默认关闭的 `STRATA_LAB_RDNA_COMBINE_CPU=1`，在普通HIP部分驻留路径中直接从映射内存读取CPU专家结果、从显存读取GPU命中结果，保持专家顺序合并。省去独立的CPU行复制与GPU命中行加和；CPU发布等待和shared gate仍使用开发版原实现，本实验不融合等待或门控。

限定top-k=10、原生专家格式、原生BF16 shared expert、vec4宽度、窗口2…8。全驻留、设备计划、远端专家、peer专家层、阶段计时／trace和负载平衡计时均回退原路径；窗口1也回退。API检查输入跨度、对齐、输出别名与窗口边界。开关须在构造／捕获会话前设置，已有计算图不会因修改环境变量而重写。

```sh
cmake --build build --target strata rdna_cpu_combine_probe -j 4
export LD_LIBRARY_PATH=/opt/rocm/lib:/opt/rocm/lib/llvm/lib
build/rdna_cpu_combine_probe 0
build/rdna_cpu_combine_probe 1
# In the controller profile, before loading:
# "env": {"STRATA_LAB_RDNA_COMBINE_CPU": "1"}
```

探针对照实际 `wait_flag_ge`→`copy_rows_from_mapped`→`moe_hit_add`→`native_moe_combine_multi` 流程，检查不同宽度、T=1…8、全CPU／全GPU及混合命中、正负输入、大数、有符号零、延迟CPU发布后的图重放和缓冲哨兵。最终探针第三次重放使用全零CPU／GPU／shared结果，避免零符号差异被其他非零项掩盖。CPU每次重放改写映射结果，避免只用已就绪数据掩盖发布或复用问题。图捕获时的首次选中日志仅证明选择入口，不是执行次数或全层覆盖统计。整机收益需看同二进制关闭／开启后的单路、双路复测。
# 0.2.16 RDNA3 实验开关

这里保存筛选计划，包括没有获得整机收益的组合。默认部署模板不因单次微基准变快而改变。先按仓库 README 构建，使用相同模型、专家画像、提示、惩罚、缓存、输出长度及功耗做开／关对照。

| 开关 | 条件与行为 | 验证范围 |
|---|---|---|
| `STRATA_LAB_RDNA_GR=1` | 选择迁移的 BF16 HC read | 两张 gfx1100，T=1…8，pending write、inject 与图重放 |
| `STRATA_LAB_RDNA_HC_Q8=1` | 同时设置 `STRATA_HC_Q8=1`、`STRATA_LAB_RDNA_GR=1`，读取上游加载器已有的 Q8_0 副本 | 对照上游 Q8 GPU 结果；未定义的 `lo` 工作区不参与比较 |
| `STRATA_GR_RDNA_A8=1` | 在上述 Q8 路径中量化激活，属于近似计算；inject 仍为 BF16 | 对照独立 CPU 标量 Q8/A8 参考；分别检查 `STRATA_GR_INJ_UP=0/1` |
| `STRATA_LAB_RDNA_MMVQ=1` | 迁移旧版稠密矩阵 wave32 布局，保持上游四组部分和的累加顺序 | Q4_0、Q5_0、Q8_0、IQ4_NL，各 1…16 列、边界形状与图重放，逐位比较 |
| `STRATA_LAB_RDNA_Q5_HEAD=1` | 将同一精确求和布局应用于2560输入、至少65536输出行的Q5_K输出头，限2…8列 | 逐位比较完整248320词表头、65537奇数行与回退形状；1列保留上游短行归约 |
| `STRATA_LAB_RDNA_KQUANTS=1` | 迁入main的gfx1100 IQ4_XS、Q6_K、IQ3_S专用核；Q4_K/Q5_K默认仅4…8列；1…8列以外回退。改变求和顺序，默认关闭 | `rdna_kquant_probe`检查上游误差、多列一致性、成对发射和图重放；不是逐位等同上游 |
| `STRATA_LAB_RDNA_KQUANT_TYPES=14,23` | 依赖上一项，按GGML类型选择内部投影核；12=Q4_K、13=Q5_K、14=Q6_K、21=IQ3_S、23=IQ4_XS。未设置时使用全部支持格式；空值、未知格式或格式错误时全部回退 | 掩码由进程首次读取后固定；未选格式与上游逐位一致，选中格式执行原容差检查 |

MMVQ 初次迁移仅用于 `g_multi_exact` 开启、`STRATA_TSUM` 路径关闭、输入维度2048…2560、输出行数至少1024、列数1…8的 small-format 调度。其余形状保留上游路径。第十轮另外增加K-quant专用核实验，没有更改MoE专家矩阵的调度。

Q5_K输出头是单独的实验开关，不依赖small-format开关。它同样要求 `g_multi_exact` 且关闭 `STRATA_TSUM`。该路径复用已有量化权重，没有额外的头权重副本；保持上游各部分和的累计次序，不采用旧Q5_K专用实现的另一种归约顺序。

HC Q8/A8 复用上游 Q8_0 权重，不分配第二份旧版平面 R8 权重。34字节量化块的码区只有2字节对齐，读取使用16位访问。没有启用旧版跨工作组自旋屏障融合。

## Fixed-zero expert PCIe graph

Set `STRATA_LAB_RDNA_ZERO_PCIE_GRAPH=1` before loading a native HIP service on a physical two-card gfx1100 split with explicit `--pcie-frac 0`. Automatic share, balance, dynamic fetch, device plans and remote/peer experts are outside the experiment. Ineligible loads keep the full graph. An armed load accepts only zero-valued `pcie_frac` request overrides. This does not remove activation transfers between the two cards.

Use `STRATA_VERIFY_NODES=1` in a separate diagnostic run to compare window and batch graph histograms by device, layer range and verifier; the verifier identity distinguishes parity graphs within a process. Leave diagnostics off during throughput tests. Run `cmake --build build --target hip_flag_order_test` and execute `build/hip_flag_order_test` with the matching ROCm runtime on idle GPUs. The [paired results](../benchmarks/2026-11-20-zero-pcie-graph.md) support enabling this only in the fixed-zero dual-card development template. Unset the switch if the service needs request-level nonzero shares.

## PLE history advance and snapshot

Set `STRATA_LAB_RDNA_PLE_HISTORY_SNAP=1` before capturing graphs to combine the row's PLE history advance and post-row snapshot. This is off by default and requires the native 512-expert fixed-zero PCIe graph. Rows that do not save a snapshot and the separate `STRATA_DF_PLE` path are unchanged. The live history and per-row snapshot retain their existing addresses, layout, and commit semantics.

```sh
cmake --build build --target strata hip_ple_history_snap_test -j 4
build/hip_ple_history_snap_test 0
build/hip_ple_history_snap_test 1
```

Run on isolated GPUs with the matching ROCm runtime. The probe compares live history and snapshots bitwise against the original kernels, including graph replays, truncated commits, and the following window. In a separate fixed-input run with `STRATA_VERIFY_NODES=1` and profiling unset, check for one fewer node per snapshotted row on the front stage and no change on the final stage. Run the token ID, rollback, and answer regression checks before timing the service with diagnostics unset. See the [b438 record](../benchmarks/2026-11-21-ple-history-snap.md).
## Fixed-zero PCIe host planner

`STRATA_LAB_RDNA_ZERO_PCIE_PLAN=1` enables indexed grouping for eligible native fixed-zero host plans (512 experts, top-10, 2–4 rows). It is separate from the fixed-zero graph switch. Build `zero_pcie_plan_test` with `STRATA_BUILD_TESTS=ON`, then run `ctest -R '^zero_pcie_plan_test$'` in the build directory for parity or run the test executable with `--bench` for CPU timing. Use `STRATA_LAB_RDNA_ZERO_PCIE_PLAN_STATS=1` only in separate diagnostic runs; the layer, width and miss-count buckets are emitted at thread exit. Leave the planner off in the default template: [the paired test](../benchmarks/2026-11-21-zero-pcie-plan.md) did not show a general throughput gain.
(PLE history advance and snapshot fusion: one node fewer per row, no service gain (off))

## Routing look-ahead off

`STRATA_LOOKAHEAD=0` disables the routing-aware file-tier prefetch worker (`RouterLookahead`), the documented upstream A/B arm. On this rig its read-ahead measures zero used bytes while the gate bookkeeping keeps running, so it was screened on the fixed-zero template. The result was mixed (Chinese dual up in every pair, code single down in every pair) and the switch was **not** adopted; see the [paired record](../benchmarks/2026-11-20-lookahead-off.md) before touching it.

## File-tier prefetch off

On the tested mapped `experts.bin` pack, `STRATA_IO_PREFETCH=0` disables both file-tier prefetch and routing look-ahead; it is not an I/O-only counterpart to `STRATA_LOOKAHEAD=0`. The [paired test](../benchmarks/2026-11-21-file-tier-prefetch-off.md) found a code-dual gain but slower single-request medians. Keep the default for the general-purpose dual-card template.

## MTP draft stream priority

`STRATA_MTP_HIP_PRIO=1` raises the drafter's stream to the highest priority (own hardware queue per ROCm's per-level allocation; default-on for gfx12, opt-in here). gfx1100 exposes a real range (1..-1), and when the switch is set the engine logs `strata mtp: draft stream priority raised (range …)`. Screened on the fixed-zero template: mixed result, **not** adopted; see the [paired record](../benchmarks/2026-11-20-mtp-prio.md).

## 运行数值探针

```sh
cmake --build build --target rdna_hc_probe rdna_mmvq_probe rdna_kquant_probe -j 4
export LD_LIBRARY_PATH=/opt/rocm/lib:/opt/rocm/lib/llvm/lib
for gpu in 0 1; do
  build/rdna_hc_probe "$gpu" bf16
  build/rdna_hc_probe "$gpu" q8
  STRATA_GR_INJ_UP=0 build/rdna_hc_probe "$gpu" a8
  STRATA_GR_INJ_UP=1 build/rdna_hc_probe "$gpu" a8
  build/rdna_mmvq_probe "$gpu"
  build/rdna_kquant_probe "$gpu"
done
```

这些探针用于核对计算与图执行，不能替代回答质量测试。微基准是热权重条件下的局部耗时，不是服务吞吐。实验对象为 gfx1100；未据此推断其他 GPU 的收益。

K-quant探针使用固定种子生成的合法量化块及实际投影维度；预先固定容差为相对L2不超过`2e-5`，最大绝对误差不超过`1e-5 + 2e-5 * max(abs(reference))`。单列／多列检查用于IQ4_XS、Q6_K和IQ3_S；Q4_K/Q5_K在1…3列走回退，因此不要求它们与另一种布局逐位相同。成对发射只用于行数为8倍数且两矩阵同格式的情况。图重放必须逐位匹配该优化核直接发射的结果。

分格式选择针对同一个GSQ-RCO IQ3_XXS模型内部的混合量化矩阵，不是更换模型为IQ4或Q6。每个线程／设备／格式首次选中时输出一条诊断日志；图捕获也会记录，因此不能将日志条数当作实际执行次数。选择示例与独立掩码校验：

```sh
# 只启用内部Q6_K和IQ4_XS投影；服务加载前设置。
export STRATA_LAB_RDNA_KQUANTS=1
export STRATA_LAB_RDNA_KQUANT_TYPES=14,23
# 探针的第二参数是独立给定的期望位掩码，8404992 = (1<<14) | (1<<23)。
build/rdna_kquant_probe 0 8404992
build/rdna_kquant_probe 1 8404992
# 无效列表应完全回退；期望掩码为0。
STRATA_LAB_RDNA_KQUANT_TYPES='23,garbage' build/rdna_kquant_probe 0 0
```

探针默认不计微基准时间；仅需局部诊断时设置`STRATA_RDNA_PROBE_BENCH=1`。顺序计时的热状态和频率可导致同一路径也出现差异，不能替代服务对照。

筛选使用768输出、每项1次，适合排除退化组合。采用候选前需要1024输出、至少3次的独立加载复测，并继续核对惩罚历史和回滚行为。单路是一个请求使用两张GPU；双路吞吐使用总输出token除以从请求发出到整批结束的时间，包含首字等待。

## BF16 HC 32列与本地量化实验

`STRATA_LAB_RDNA_GR_Q32=1` 依赖 `STRATA_LAB_RDNA_GR=1`，默认关闭，仅改变本地BF16 HC读取的up阶段。工作组从16列／256线程改为32列／512线程，每组独占一个q8_1块，可在调用方允许时同时产生量化图像，省去独立量化发射。逐行求和顺序保持不变；Q8/A8权重路径不变。没有跨工作组计数或自旋屏障。

关闭 `STRATA_QFUSE_HC_LOCAL` 且未提供计数器时，仍须返回“未融合量化”，交给调用方执行独立量化。新增探针经实际调度入口检查这个回退，避免将未写入图像误报为已生成。

新版草稿层的 `mtp_qfuse_hc_on()` 只读取 `STRATA_QFUSE_HC`，不读取 `STRATA_QFUSE`，gfx1100默认关闭。要测草稿层的无计数器融合，另设 `STRATA_QFUSE_HC=1`；只开32列开关并不能证明草稿层已产生融合量化图像。首次选中日志标明T与图像指针是否存在，图捕获也会记录，同一T后续调用不重复输出，不能据第一条日志推断所有层的路径或执行次数。

```sh
cmake --build build --target strata rdna_hc_probe -j 4
export LD_LIBRARY_PATH=/opt/rocm/lib:/opt/rocm/lib/llvm/lib
for gpu in 0 1; do
  build/rdna_hc_probe "$gpu" bf16
  build/rdna_hc_probe "$gpu" q32plain
  build/rdna_hc_probe "$gpu" q32
  STRATA_RDNA_PROBE_ZERO=1 build/rdna_hc_probe "$gpu" q32
  STRATA_QFUSE_HC_LOCAL=0 build/rdna_hc_probe "$gpu" q32
done
```

`q32plain`／`q32` 对T=1…8、apply与inject开关检查五个浮点输出与旧16列核逐位一致；`q32`另将图像字节与旧核后单独运行的原生q8_1量化逐位比较，并检查缓冲尾部哨兵及图重放。零输入检查量化零值边界。微基准两臂包含的量化工作可能不同，不能从其顺序计时推算整机收益。

## CPU mapped-row combine 实验

开发版配置继承的 `STRATA_COMBINE_CPU=1` 没有对应实现，不会打开现役旧版的CPU结果合并优化。新增独立、默认关闭的 `STRATA_LAB_RDNA_COMBINE_CPU=1`，在普通HIP部分驻留路径中直接从映射内存读取CPU专家结果、从显存读取GPU命中结果，保持专家顺序合并。省去独立的CPU行复制与GPU命中行加和；CPU发布等待和shared gate仍使用开发版原实现，本实验不融合等待或门控。

限定top-k=10、原生专家格式、原生BF16 shared expert、vec4宽度、窗口2…8。全驻留、设备计划、远端专家、peer专家层、阶段计时／trace和负载平衡计时均回退原路径；窗口1也回退。API检查输入跨度、对齐、输出别名与窗口边界。开关须在构造／捕获会话前设置，已有计算图不会因修改环境变量而重写。

```sh
cmake --build build --target strata rdna_cpu_combine_probe -j 4
export LD_LIBRARY_PATH=/opt/rocm/lib:/opt/rocm/lib/llvm/lib
build/rdna_cpu_combine_probe 0
build/rdna_cpu_combine_probe 1
# In the controller profile, before loading:
# "env": {"STRATA_LAB_RDNA_COMBINE_CPU": "1"}
```

探针对照实际 `wait_flag_ge`→`copy_rows_from_mapped`→`moe_hit_add`→`native_moe_combine_multi` 流程，检查不同宽度、T=1…8、全CPU／全GPU及混合命中、正负输入、大数、有符号零、延迟CPU发布后的图重放和缓冲哨兵。最终探针第三次重放使用全零CPU／GPU／shared结果，避免零符号差异被其他非零项掩盖。CPU每次重放改写映射结果，避免只用已就绪数据掩盖发布或复用问题。图捕获时的首次选中日志仅证明选择入口，不是执行次数或全层覆盖统计。整机收益需看同二进制关闭／开启后的单路、双路复测。
(Pinned penalty-history staging: removing the post-upload stream sync costs the Chinese lanes (off))
