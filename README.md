# Fireline / 火力对决

UE 5.8 桌面 FPS。当前开发基线：**手柄阶段重建基线** `gamepad-reconstructed-20260927`。

用户选择从手柄适配阶段重新推进。此基线按当时入口和资源重建，并非当时完整源码的逐字节回滚；后续动作实验未启用。旧工程、正式 0.1.10 和失败实验留在原目录，不是此仓库的默认运行内容。

## 范围

最新独立动作候选：桌面「火力对决／**测试版·M4多方向走跑.app**」。WASD移动、Shift慢跑、Q/E改变朝向，鼠标环绕，1–4观察方向，R回到起点。原ALS完整图驱动八方向与转身，保留当前比例模型和M4双手接触；仅M4平地无操作测试，等待视觉评价。不是默认游戏更新。前向起停入口保留。见 [验证记录](unreal/Fireline/Docs/Validation/live-directional/README.md) 与 [后续顺序](unreal/Fireline/Docs/NEXT_DEVELOPMENT.md)。

- 保留原片四方向起停、侧步／足底与枪托修正、既有 M4/MP7/爪刀和手柄输入。
- 固定配置在 `baseline.json`。MatchedGround、CombatGround、SourceBody、SideJog、Pivot 均关闭。
- 旧实验代码为保持共有依赖暂时保留；不能仅凭文件存在判断它启用。
- 手柄历史验证为引擎注入；真实手柄、Windows 手感和全部动作美术尚未验收。
- 共有极端瞄准脊柱分配、肘部拟合罚项无法证明与手柄历史时点相同，列为后续对照项，不虚称精确恢复。

## 获取与构建

这个公开仓库保存源码、配置、归属信息和资源校验清单。**第三方模型、动画和 UE 资源没有随仓库分发，单独 clone 不能直接运行。** 本机独立工程已复制完整 Content；原始资源及历史证据继续保存在原工程。资源清单 `unreal/Fireline/Docs/local-assets.json` 可校验本机快照。异机搭建需先合法取得对应资源并恢复清单里的 Content；当前尚无自动下载流程。

1. 安装 UE 5.8、对应平台 C++ 编译工具。
2. 恢复本机资源后运行 `python3 scripts/verify_assets.py`。
3. 用 UE Build.sh / Build.bat 构建 `FirelineEditor Development`，项目为 `unreal/Fireline/Fireline.uproject`。
4. Mac 运行 `python3 scripts/run.py`；其他平台用 `--editor` 指定 UnrealEditor 路径。
5. Mac 安装可双击入口：`python3 scripts/install_launcher.py`。入口：桌面「火力对决／开发版·手柄基线.app」。V 切换视角；测试移动、起停、切枪、换弹及手柄设置。

验证命令：`python3 scripts/run.py --audit gamepad`、`--audit weapons`、`--audit mouse`。日志在本机 Saved，验证结果见 `unreal/Fireline/Docs/BASELINE_VALIDATION.md`。自动检查不代表动作视觉通过。

2026-09-27：[M4 持枪层修正](unreal/Fireline/Docs/Validation/carry-layer-ownership/README.md)保留下半身起停结果，修复 ContactCarry 全身覆盖；开发入口已更新。可用 `python3 scripts/run.py --audit carry` 重跑短循环，使用本地原生截图对照页检查。

2026-09-27：[全动作诊断与接触连续性](unreal/Fireline/Docs/Validation/source-arm-continuity/README.md)：保留 MP7 适配修正的连续性与动作结束恢复，失败的肘部改写已撤回；整套第三人称体态仍未完成。开发入口已启用该有限修正。

2026-09-27：[M4完整身体动作对照](unreal/Fireline/Docs/Validation/whole-carry-review/README.md)。桌面「测试版·M4全身动作对照.app」回放原版ALS完整图与Ryan/DJ适配候选，支持慢放和四向观察；仅待机/前走/停下，未接入默认游戏，等待视觉评价。

2026-09-27：[角色比例与袖口连接修订](unreal/Fireline/Docs/Validation/body-proportion/README.md)。上一版被用户指出胸部过大与穿模；桌面「测试版·角色比例修正.app」对照原版ALS、修改前、修改后。缩小胸肩并同步移动骨骼，保持原DJ手形，闭合袖口连接；仅独立M4携枪片段，尚未更新默认游戏。

同日按用户要求轻微收整髋部和腿部轮廓，保留腿长、膝踝关节、靴底和既有动作；同一比例演示入口已更新。[后续开发顺序](unreal/Fireline/Docs/NEXT_DEVELOPMENT.md)：先将该回放改造成可操作的独立M4起停测试，再做多方向、武器操作、完整跑跳滑铲和其他武器。

## 版本规则

2026-09-27：[成熟动作系统完整对照](unreal/Fireline/Docs/Research/mature-animation-systems/README.md)。用户再次否决当前动作观感；新增下载ALS完整工程、读取原片／曲线／动画图并对照Lyra与当前双重手臂重建。仅研究，游戏运行时未改；桌面「参考·ALS原版动作.app」是独立原版观察入口。

2026-09-27：[FPS人物体态研究](unreal/Fireline/Docs/Research/fps-posture/README.md)包含制作组资料、21段原始动画的重新测量、当前求解层风险与建议。仅研究，未更换动作或启用后续实验。

每次修改围绕一个明确问题；先记录现象再实施。编译、功能回归、实际画面分别记录。通过后提交并标记版本；未验收动作不得更换默认基线。费用为零；不启用付费服务。所有模型来源及第三方权利见 asset-lock.json 和 Docs/Attribution，仓库不授予第三方素材额外许可。
