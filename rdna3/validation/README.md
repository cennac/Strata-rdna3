# 开发版验证

两张gfx1100分别通过160项HC read字段检查，T=1…8、pending write开/关、inject开/关。数值输出见同目录txt；relative L2 < 1e-4。构建与运行：

```sh
python3 rdna3/build.py --rocm /opt/rocm --jobs 4
cmake --build build --target rdna_hc_probe -j 4
LD_LIBRARY_PATH=/opt/rocm/lib:/opt/rocm/lib/llvm/lib build/rdna_hc_probe 0
LD_LIBRARY_PATH=/opt/rocm/lib:/opt/rocm/lib/llvm/lib build/rdna_hc_probe 1
```

惩罚流水线在冻结专家自适应和后缀草稿的条件下，完成中文与代码各384输出的串行重复、流水线、强制回滚对照，文本一致。完整采样/续聊覆盖尚未完成，不能作为上游全平台合并依据。

完整整机速度和1024预算的质量冒烟检查见[测试记录](../benchmarks/2026-11-19.md)。算术题仍未通过；默认开关关闭，开发模板显式开启本机验证组合。
