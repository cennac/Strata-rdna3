# 0.2.16 RDNA3 实验开关

这里保存筛选计划，包括没有获得整机收益的组合。默认部署模板不因单次微基准变快而改变。先按仓库 README 构建，使用相同模型、专家画像、提示、惩罚、缓存、输出长度及功耗做开／关对照。

| 开关 | 条件与行为 | 验证范围 |
|---|---|---|
| `STRATA_LAB_RDNA_GR=1` | 选择迁移的 BF16 HC read | 两张 gfx1100，T=1…8，pending write、inject 与图重放 |
| `STRATA_LAB_RDNA_HC_Q8=1` | 同时设置 `STRATA_HC_Q8=1`、`STRATA_LAB_RDNA_GR=1`，读取上游加载器已有的 Q8_0 副本 | 对照上游 Q8 GPU 结果；未定义的 `lo` 工作区不参与比较 |
| `STRATA_GR_RDNA_A8=1` | 在上述 Q8 路径中量化激活，属于近似计算；inject 仍为 BF16 | 对照独立 CPU 标量 Q8/A8 参考；分别检查 `STRATA_GR_INJ_UP=0/1` |
| `STRATA_LAB_RDNA_MMVQ=1` | 迁移旧版稠密矩阵 wave32 布局，保持上游四组部分和的累加顺序 | Q4_0、Q5_0、Q8_0、IQ4_NL，各 1…16 列、边界形状与图重放，逐位比较 |
| `STRATA_LAB_RDNA_Q5_HEAD=1` | 将同一精确求和布局应用于2560输入、至少65536输出行的Q5_K输出头，限2…8列 | 逐位比较完整248320词表头、65537奇数行与回退形状；1列保留上游短行归约 |

MMVQ 初次迁移仅用于 `g_multi_exact` 开启、`STRATA_TSUM` 路径关闭、输入维度2048…2560、输出行数至少1024、列数1…8的 small-format 调度。其余形状保留上游路径。没有迁移旧版 Q6_K 专用内核，也没有更改专家矩阵的调度。

Q5_K输出头是单独的实验开关，不依赖small-format开关。它同样要求 `g_multi_exact` 且关闭 `STRATA_TSUM`。该路径复用已有量化权重，没有额外的头权重副本；保持上游各部分和的累计次序，不采用旧Q5_K专用实现的另一种归约顺序。

HC Q8/A8 复用上游 Q8_0 权重，不分配第二份旧版平面 R8 权重。34字节量化块的码区只有2字节对齐，读取使用16位访问。没有启用旧版跨工作组自旋屏障融合。

## 运行数值探针

```sh
cmake --build build --target rdna_hc_probe rdna_mmvq_probe -j 4
export LD_LIBRARY_PATH=/opt/rocm/lib:/opt/rocm/lib/llvm/lib
for gpu in 0 1; do
  build/rdna_hc_probe "$gpu" bf16
  build/rdna_hc_probe "$gpu" q8
  STRATA_GR_INJ_UP=0 build/rdna_hc_probe "$gpu" a8
  STRATA_GR_INJ_UP=1 build/rdna_hc_probe "$gpu" a8
  build/rdna_mmvq_probe "$gpu"
done
```

这些探针用于核对计算与图执行，不能替代回答质量测试。微基准是热权重条件下的局部耗时，不是服务吞吐。实验对象为 gfx1100；未据此推断其他 GPU 的收益。

筛选使用768输出、每项1次，适合排除退化组合。采用候选前需要1024输出、至少3次的独立加载复测，并继续核对惩罚历史和回滚行为。单路是一个请求使用两张GPU；双路吞吐使用总输出token除以从请求发出到整批结束的时间，包含首字等待。
