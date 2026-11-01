# TinyTheme

Windows 11 浅色 / 深色主题一键切换工具。单文件、零运行时依赖、免安装。

> 当前状态：设计阶段，代码尚未开工。文档先行。

## 文档入口

- [文档索引与阅读指南](docs/README.md)
- 产品：[概览](docs/product/01-overview.md) · [用户与场景](docs/product/02-users-and-scenarios.md) · [功能规格](docs/product/03-feature-spec.md) · [交互设计](docs/product/04-interaction-design.md) · [路线图](docs/product/05-roadmap.md) · [图标设计](docs/product/06-icon-design.md)
- 技术：[架构](docs/engineering/01-architecture.md) · [技术选型](docs/engineering/02-tech-stack.md) · [主题切换](docs/engineering/03-theme-switching.md) · [模块设计](docs/engineering/04-modules.md) · [配置与运行时](docs/engineering/05-config-and-runtime.md) · [构建](docs/engineering/06-build.md) · [测试](docs/engineering/07-testing.md) · [交付](docs/engineering/08-delivery.md) · [规范](docs/engineering/09-conventions.md) · [图标生成](docs/engineering/10-icon-pipeline.md)
- 决策记录：[0001](docs/decisions/0001-cpp-win32-stack.md) · [0002](docs/decisions/0002-zero-dependency.md) · [0003](docs/decisions/0003-tray-first.md)

## 快速了解

| 项 | 内容 |
|---|---|
| 解决的问题 | Win11 切换深浅色要翻三层设置，没有快捷键 |
| 核心交互 | 托盘图标左键单击切换（P0）；全局热键 `Ctrl+Alt+D`（P1，v0.2） |
| 发布物 | 单个 exe，免安装，普通用户权限 |
| 支持系统 | Windows 10 1809+ / Windows 11 |
| 许可证 | 待定（倾向 MIT） |
