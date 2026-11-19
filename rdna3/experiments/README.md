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
