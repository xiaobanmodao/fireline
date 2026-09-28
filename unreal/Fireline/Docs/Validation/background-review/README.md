# 不占用桌面输入的原生截图检查（2026-09-28）

已验证 UE 5.8 / macOS 的离屏 Metal 渲染。没有建立虚拟机或单独的 macOS Space，也没有模拟实体鼠标键盘；测试过程仍消耗本机 CPU/GPU，不能承诺零性能影响。

## 已完成的验证

本次运行恢复后的「M4移动瞄准修正」候选，5秒自动姿态序列，30 Hz，1100×850，保存正/右/背/左四张原生截图。进程正常退出，总耗时21.9秒；截图已逐张查看，模型和枪械均正常渲染，仍可见现有眼线高于机瞄的问题。**这证明截图工作流可用，不证明动作质量全部通过。**

引擎运行时检查OS窗口句柄为空，日志：

```
LIVE_CARRY_BACKGROUND native_window=0 scripted_input=1
LIVE_CARRY_AUDIT_COMPLETE
```

`-RenderOffScreen` 使用 Slate 通用窗口；`-MetalOffscreenOnly` 保留GPU渲染而不向桌面呈现。`-nosplash` 避免启动画面，审计模式使用UI输入模式和脚本状态，不锁鼠标。没有用NullRHI的无画面运行冒充渲染验证。

## 运行方式

先构建现有独立候选（不加 `--install`，不自动打开窗口）：

```sh
python3 scripts/prepare_live_carry_study.py --presentation --build
python3 scripts/run_background_review.py
```

每次自动生成新的证据标签；可以指定 `--label <新名称>`。已有同名证据会拒绝覆盖。若已有 UnrealEditor 进程，脚本会退出而保留该进程。运行上限和异常退出只结束它自己创建的子进程。

`--full` 请求原有77秒/19组动作检查序列，预期38张截图；`--fps 30/60/120`选择模拟采样率。默认渲染上限仍为30 FPS以减轻占用。**本次只实际运行了默认四面短序列，没有重新执行完整19组**；原姿态代码未改，之前完整记录继续保留。

成功返回截图目录、日志位置与 `background-review.json`。原生日志断言、输出数量与PNG非空检查均通过后才报告成功；图片仍需人工/视觉检查。

## 实际证据

本机目录：`Saved/MatureMotionResearch/LiveCarryProject/Saved/LiveAim/background-four-view-30/`，包含 `shot-00.png` 至 `shot-03.png`、`states.csv`、`poses.csv`。日志在同工程 `Saved/BackgroundReview/background-four-view.log`。`manifest.json`保存文件哈希与验证范围，不分发第三方素材截图。

现有可体验入口未更新姿态：桌面「火力对决／测试版·M4移动瞄准修正.app」。需要交互体验时由用户打开；后台截图不替代真实输入延迟、声音或交互手感验收。
