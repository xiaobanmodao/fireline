# 头盔比例恢复 — 2026-09-30

用户否决上一版后，本次**撤回错误轮廓，恢复原 Ryan 头盔**。这不是新的头颈接触解算，也不是全动作修复。

## 体验入口

桌面「火力对决／测试版·头颈轮廓对照.app」，双击打开。完整路径：

`/Users/hht/Desktop/火力对决/测试版·头颈轮廓对照.app`

默认显示恢复的原头盔；H临时切到上一版被否决的扁平头盔，再按H返回。F或右键瞄准；1–4四面；Z近景/全身；WASD/Shift走跑；↑↓俯仰；Q/E朝向；6自动演示；O机瞄/Coyote；5成熟原版对照。7显示辅助线，紫色是设计眼点代理，不是真实眼骨。没有射击和换弹。

入口指向共享可变 LiveCarryProject，不是冻结版本。默认游戏、第一人称、既有正式版和作者选择没有更换。

## 原因与实际改动

上一版把 ALS 裸头的眼球和头颈高度映射到包覆式 Ryan 头盔，却保留接近原来的横向宽度。头顶从202.32cm压低到196.91cm，面罩附近地标从190cm压低到181.96cm。头部主权重区域高度从28.90cm缩成23.49cm，减少约18.7%；宽高比从0.968增到1.193。这使头盔扁宽，面罩和下颌比例变形。裸头地标不能直接作为另一角色头盔的外壳标准。

本次直接复用 `/Game/BodyProportionStudy/SK_RyanProportion` 的原头盔表面，取消生成轮廓作为默认显示。没有新雕刻、非等比压缩、骨骼改动或重新蒙皮；建模顶点4682、三角面9360、原生渲染顶点9936。其资产SHA256记录在 restoration.json。H对照仍使用被否决的 `SK_RyanHeadContourRounded`；该失败模型实际渲染顶点为10003，旧报告的9977来自更早的候选，不能沿用为最终统计。

两种网格切换后立即恢复当前131骨骼的同帧姿态，原生逐骨检查用于确认换网格没有插入参考姿势。手枪接触、Retarget、输入和身体动作算法没有改写。轮廓恢复不等于正确贴腮：眼线和枪托需要通过完整肩臂、头颈姿态与真实接触表面解决，不能再靠压扁头盔迁就。

## 验证与边界

UE5.8构建成功；后台原生 Metal 四面近景已经逐图查看。走跑、瞄准、转向完整序列和同帧网格切换的结果记录在 restoration.json。截图不是离线摆拍；后台窗口句柄为空，无实体键鼠操作。没有自动把窗口置前。

当前 Presentation 姿态仍有头颈/枪械及肩臂接触缺陷。上一轮57帧头颈抽查不覆盖全序列；补充肩臂/躯干检查也发现非相邻三角面交叉，近肩混合权重区域需要进一步区分接缝与真实侵入。旧文档“零躯干相交”的检查排除了肩关节附近区域，不能外推为完整人物零穿模。本次不发布该声明，不推进换弹或正式集成。

## 保留的失败记录

- 初版头颈整体下移造成衣领自穿插，保留在本地 HeadContourStudy/InitialContour。
- 第二版保留衣领但顶部缩窄，呈铃铛形，被用户否决。
- 第三版 Rounded 高度压缩形成扁宽头盔，被用户再次否决。此前“圆形轮廓解决外形”的结论撤回。
- 离线静止姿态即使头颈/枪械相交为零，仍有15对肩臂/躯干相交，因此从未接入运行时。
- 一组枪托选定表面的完整顶点/面及边/边最近距离为约0.4804cm；旧顶点/面结果1.7325cm只是上界，不是完整接触验收。

旧 measurements.json 只作失败历史，不代表当前有效模型；其中混用的早期导入位置/权重统计没有重新宣称为 Rounded 最终导入验证。原始模型、动画与全部既有失败资源保留。

## 复现

`build_head_contour_geometry.py` 是被否决的历史构造脚本，不用于当前恢复。无需 Blender 重建或导入新模型。

```sh
python3 scripts/prepare_live_carry_study.py --head-contour --build --install
python3 scripts/run_background_review.py --head-contour --label fresh-restored-four
python3 scripts/run_background_review.py --head-contour --full --label fresh-restored-moving
python3 scripts/run_background_review.py --head-contour --head-swap --label fresh-restored-swap
python3 scripts/verify_assets.py
```

下一步先建立一帧完整肩—肘—腕—握持—枪托—头颈接触基准，保持原头盔轮廓，检查真实表面、接缝和四面观感；没有成立的静止基准前不扩展换弹。
