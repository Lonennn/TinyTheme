# 文档索引

> 状态：草案 v0.1 ｜ 更新：2026-11-01

## 目录结构

| 目录 | 内容 | 主要读者 |
|---|---|---|
| `product/` | 为什么做、给谁做、做成什么样 | 产品、设计、开发 |
| `engineering/` | 怎么实现、怎么构建、怎么交付 | 开发、测试、发布 |
| `decisions/` | 关键技术决策记录（ADR） | 所有人 |

## 阅读顺序

- 第一次接触：`product/01-overview.md` → `product/03-feature-spec.md` → `engineering/01-architecture.md` → `engineering/02-tech-stack.md`
- 参与开发：`engineering/` 全部，按编号顺序读一遍再动手
- 只关心交互：`product/04-interaction-design.md`
- 只关心图标：`product/06-icon-design.md` + `engineering/10-icon-pipeline.md`
- 只关心构建发布：`engineering/06-build.md` + `engineering/08-delivery.md`
- 想知道"为什么这么定"：`decisions/` 下的三篇 ADR

## 文档约定

- 每篇文档头部标注状态（草案 / 评审通过 / 已实现）与最后更新日期。
- 需求用 `F-xx` 编号，验收标准用 `AC-xx` 编号，测试用例用 `TC-xx` 编号；跨文档引用编号，不引用标题。
- 术语首次出现时使用术语表定义，之后保持同一写法（例如统一写"系统主题"，不混用"系统模式"）。
- 文档与代码冲突时以文档为准；实现方式变更必须先改文档，再改代码。
- 文档中的体积、内存、耗时均为目标值，不是实测值；实测数据记录在 `engineering/06-build.md` 的验证章节。体积只作目标与记录，不设硬性上限，也不作为发布门禁。

## 术语表

| 术语 | 含义 |
|---|---|
| 模式（Mode） | 浅色 / 深色 / 自动，三种状态之一 |
| 临时覆盖（Override） | 自动模式下用户手动切换后进入的临时状态，到下一个计划切换点自动恢复自动模式 |
| 应用主题 | 注册表 `AppsUseLightTheme`，控制应用窗口的浅色 / 深色 |
| 系统主题 | 注册表 `SystemUsesLightTheme`，控制任务栏、开始菜单、通知中心 |
| 广播 | 向所有顶层窗口发送 `WM_SETTINGCHANGE` 并携带 `ImmersiveColorSet`，通知主题已变更 |
| 编排（Orchestration） | 切换主题时联动壁纸、强调色、编辑器主题等，属于 v0.3 规划 |
| 托盘 | 任务栏右下角通知区域（Notification Area） |
| 计划切换点 | 自动模式下预定的下一次切换时刻 |
