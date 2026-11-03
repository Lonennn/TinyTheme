# AGENTS.md

给在本仓库工作的编码代理的入口。人类读者从 [README.md](README.md) 开始，完整的文档地图在 [docs/README.md](docs/README.md)。

## 1. 项目

Windows 托盘小工具：点一下在浅色 / 深色之间切换。C++23 + 原生 Win32，无第三方运行时依赖，产物是单个静态链接的 x64 / ARM64 exe。

## 2. 开工前按顺序读

| 文档 | 解决什么 |
|---|---|
| [docs/README.md](docs/README.md) | 文档分工与边界：“行为与契约以文档为准，接口与参数以代码为准” |
| [09-conventions](docs/engineering/09-conventions.md) | 命名、格式、错误处理、RAII、提交信息、本地钩子 |
| [06-build](docs/engineering/06-build.md) | 构建预设、生成器选择、配置取舍与已知的坑 |
| [07-testing](docs/engineering/07-testing.md) | 哪些检查必须绿、怎么跑 |
| [08-delivery](docs/engineering/08-delivery.md) | CI 编译矩阵、发布与签名流程 |

只改产品行为，先读 [03-feature-spec](docs/product/03-feature-spec.md) 与 [04-interaction-design](docs/product/04-interaction-design.md)。

## 3. 改完之后

- 至少跑一遍 `x64-release` 与 `x64-debug` 的构建和 `ctest`。
- 改过 C++ 代码顺手执行 `clang-format --style=file -i`；`pre-commit` 也会做，但别指望它。
- 行为变了先改文档再改代码；接口变了要删掉文档里过期的描述，而不是补一份副本（见 [docs/README.md](docs/README.md) 的“文档约定”）。
- 提交信息用 `type(scope): subject`，标题不超过 72 字符，一个提交只做一件事。
