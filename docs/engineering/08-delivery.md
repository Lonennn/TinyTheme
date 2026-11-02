# 交付

> 状态：草案 v0.1 ｜ 更新：2026-11-01

## 1. 版本管理

### 分支模型

- 主干开发（trunk-based），`main` 始终可构建、可发布。
- 功能分支生命周期不超过 3 天，合并前必须通过 CI。
- 发布打 tag：`v0.1.0`，tag 必须与 `CMakeLists.txt` 中的版本一致。
- 不使用长期维护分支；如需修复旧版本，从 tag 拉 `hotfix/x.y.z` 分支。

### 版本号

语义化版本 `MAJOR.MINOR.PATCH`：

| 位 | 递增条件 |
|---|---|
| MAJOR | 配置格式不兼容、行为破坏性变更 |
| MINOR | 新增功能 |
| PATCH | 缺陷修复、文案调整 |

预发布版本使用 `-beta.1` 后缀，不推送 winget。

## 2. CI 流水线

定义在 `.github/workflows/ci.yml`，不在这里复制。要点：

| 阶段 | 触发 | 内容 |
|---|---|---|
| 校验 | PR / push | 格式检查、构建、单元测试、依赖与导入检查、体积记录 |
| 打包 | tag | 构建 x64 与 ARM64、生成 zip、生成 SHA256 |
| 签名 | tag | 对 exe 与 zip 签名 |
| 发布 | tag | 创建 GitHub Release、上传产物、更新 CHANGELOG |
| 分发 | 发布后 | 提交 winget manifest、更新 Scoop bucket |

三条容易忘的约束：

- CI 用 **Ninja** 预设，因为 Visual Studio 生成器不产出 `compile_commands.json`，clang-tidy 门禁会失效。
- 体积只记录不阻断，作为版本间对比基线。
- ARM64 工件是**跨编译**，`vcvarsall` 里写作 `amd64_arm64`。裸 `arm64` 表示在 ARM64 宿主机上原生编译，x64 runner 上既不报错也拿不到编译器；目标工具集缺失时它还会静默退回 x64 编译器，所以构建前要核对 `cl` 的目标架构。

## 3. 代码签名

| 项 | 说明 |
|---|---|
| 服务 | Azure Trusted Signing 或 SignPath（开源项目免费额度） |
| 算法 | SHA256，时间戳使用 RFC 3161 |
| 对象 | `TinyTheme.exe` 与发布 zip |
| 校验 | `Get-AuthenticodeSignature` 返回 `Valid` |
| 失败处理 | 签名失败则不发布，不允许发布未签名产物 |

未签名构建仅用于本地开发，不对外分发。

## 4. 分发

### GitHub Releases

- 产物：`TinyTheme-v0.1.0-x64.zip`、`TinyTheme-v0.1.0-arm64.zip`、`SHA256SUMS.txt`。
- zip 内容与打包方式见 [构建 §7](06-build.md#7-打包)。
- Release 说明包含：变更摘要、已知问题、系统要求，并引用功能编号（如 `F-03`）。

### winget

`wingetcreate update TinyTheme.TinyTheme --version <版本> --urls <zip 地址> --submit`，
manifest 用 `portable` 类型。

### Scoop

在自有 bucket 维护 `tinytheme.json`，`checkver` 指向 GitHub Releases。

### 暂不提供

Microsoft Store（MSIX 与审核成本高）、安装器（保持绿色版）、自动更新。

## 5. 发布检查清单

- [ ] `main` 最新提交 CI 全绿
- [ ] 版本号已更新（`CMakeLists.txt`）
- [ ] `CHANGELOG.md` 已补充
- [ ] 测试矩阵主路径通过
- [ ] 体积、内存、启动已实测并记录
- [ ] 产物已签名并通过校验
- [ ] Release 说明已撰写
- [ ] winget / Scoop manifest 已提交
- [ ] 官网 / README 下载链接已更新

## 6. 回滚

| 场景 | 操作 |
|---|---|
| 发布后发现 P0 缺陷 | 将 Release 标记为 pre-release 或删除，重新发布上一版本 tag |
| winget 已收录错误版本 | 提交 manifest 修正 PR，说明原因 |
| 配置格式变更导致回退不兼容 | 新版本保留旧格式读取能力，至少两个 minor 版本 |

## 7. 更新机制

- v1 之前不实现自动更新；用户手动下载覆盖 exe，配置与日志保留。
- v0.2 起在"关于"菜单提供"检查更新"链接，打开 GitHub Releases 页面（v0.1 无"关于"项）。

## 8. 文档与发布同步

- 功能变更必须同步更新 `docs/product/03-feature-spec.md` 与 `docs/engineering/` 对应章节。
- 发布说明引用文档中的功能编号（如 `F-03`），便于追溯。
