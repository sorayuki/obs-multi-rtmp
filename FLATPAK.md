# OBS Multi RTMP 的 Flatpak 构建与安装

本文记录如何把本仓库构建为 OBS Studio 的 Flatpak 插件扩展，并生成可用
`flatpak install` 安装的单文件 bundle。以下流程于 2026-07-31 在 WSL2 的
openSUSE Tumbleweed x86_64 环境中验证。

## 官方 OBS 的构建方式

OBS 官方源码在
[`build-aux/com.obsproject.Studio.json`](https://github.com/obsproject/obs-studio/blob/master/build-aux/com.obsproject.Studio.json)
维护 Flatpak 清单。当前清单的关键设置如下：

- 应用 ID 为 `com.obsproject.Studio`。
- 基础运行时和 SDK 分别为 `org.freedesktop.Platform//25.08` 与
  `org.freedesktop.Sdk//25.08`。`25.08` 是 Freedesktop 运行时分支，不是 OBS
  版本。
- OBS 声明了 `com.obsproject.Studio.Plugin` 扩展点，扩展根目录是
  `/app/plugins`。
- 每个插件扩展拥有独立子目录；OBS 把其中的 `lib/obs-plugins` 和
  `share/obs/obs-plugins` 合并到应用可见目录，并把 `lib` 加入动态库搜索路径。
- 官方 OBS 在 Flatpak 沙箱内用 CMake/Ninja 编译，并把 OBS 依赖和 Qt 一起放入
  应用/runtime。

Flathub 上现有插件（例如
[`AitumMultistream`](https://github.com/flathub/com.obsproject.Studio.Plugin.AitumMultistream)）
采用相同模式：以 `com.obsproject.Studio//stable` 为 runtime、以匹配的
Freedesktop SDK 编译，并设置 `build-extension: true`。因此本项目打包为插件扩展，
而不是再复制一套 OBS 应用。

本仓库的清单位于
`flatpak/com.obsproject.Studio.Plugin.MultiRTMP.yml`，扩展 ID 是
`com.obsproject.Studio.Plugin.MultiRTMP`，安装前缀是
`/app/plugins/MultiRTMP`。清单显式设置 `CMAKE_INSTALL_LIBDIR=lib`，使插件最终
位于扩展点要求的 `lib/obs-plugins`，而不是 Ubuntu DEB 使用的 multiarch 目录。

## 准备 openSUSE WSL

以 root 安装工具：

```powershell
wsl -u root -e zypper --non-interactive install flatpak flatpak-builder git cmake ninja
```

某些精简 openSUSE WSL 根文件系统没有 `/etc/mtab`，会使 `flatpak-builder` 在
清理 `rofiles-fuse` 时失败。确认该文件缺失后补上标准链接：

```powershell
wsl -u root -e sh -lc 'test -e /etc/mtab || ln -s /proc/self/mounts /etc/mtab'
```

添加用户级 Flathub，并安装官方 OBS：

```powershell
wsl -e sh -lc 'flatpak --user remote-add --if-not-exists flathub https://flathub.org/repo/flathub.flatpakrepo'
wsl -e sh -lc 'flatpak --user install -y flathub com.obsproject.Studio'
```

`flatpak-builder --install-deps-from=flathub` 会按清单自动安装缺少的
`org.freedesktop.Sdk//25.08`。OBS 和 SDK 应保持同一基础运行时分支；OBS 官方更新
该分支时，也要同步修改插件清单中的 `sdk`。

## 构建 bundle

从 Windows PowerShell 调用仓库内脚本：

```powershell
wsl -e bash /mnt/d/dev/obs-dev/obs-multi-rtmp/flatpak/build.sh
```

脚本会执行以下工作：

1. 从本地 remote 列表确认用户级 Flathub 存在，仅在缺失时联网添加。
2. 在 Flatpak SDK 沙箱内编译本地源码。
3. 导出临时 OSTree 仓库到 WSL 用户缓存。
4. 生成 `release/obs-multi-rtmp.flatpak` 单文件 bundle。

临时 build/state/OSTree repo 位于 WSL 的
`~/.cache/obs-multi-rtmp-flatpak`，可整目录删除后重新构建。不要把这些目录放在
`/mnt/c` 或 `/mnt/d`：Flatpak-builder 的 `rofiles-fuse` 不能挂载到 WSL DrvFS。
最终 bundle 位于已被现有 `.gitignore` 排除的 `release` 目录。

## GitHub Actions 构建

`.github/workflows/build-project.yaml` 中的 `flatpak-build` 是独立 job，不加入
Ubuntu 24.04/26.04 的原生包矩阵。它运行在 Flathub 的
`freedesktop-25.08` 特权容器中，并使用固定 commit 的
`flatpak-github-actions/flatpak-builder` action 构建本清单。

该 action 会识别清单中的 `build-extension: true`，用 `--runtime` 生成插件扩展
bundle，并上传名为
`obs-multi-rtmp-<版本>-flatpak-x86_64-<commit>` 的 workflow artifact。

在 tag 构建中，`.github/workflows/push.yaml` 会下载该 artifact，把 `.flatpak`
加入 `CHECKSUMS.txt`，并附加到草稿 GitHub Release。普通 workflow dispatch 只生成
Actions artifact，不创建 Release。

## 安装和更新插件

安装首次构建的 bundle：

```powershell
wsl -e flatpak --user install -y /mnt/d/dev/obs-dev/obs-multi-rtmp/release/obs-multi-rtmp.flatpak
```

重新构建后覆盖安装：

```powershell
wsl -e flatpak --user install --reinstall -y /mnt/d/dev/obs-dev/obs-multi-rtmp/release/obs-multi-rtmp.flatpak
```

检查 OBS 和插件扩展：

```powershell
wsl -e flatpak --user info com.obsproject.Studio
wsl -e flatpak --user info com.obsproject.Studio.Plugin.MultiRTMP
wsl -e flatpak --user list --runtime
```

若 WSLg 可用，可运行：

```powershell
wsl -e flatpak run com.obsproject.Studio
```

在 OBS 的 `Help > Log Files > View Current Log` 中搜索 `obs-multi-rtmp`，可确认
动态库和本地化资源已经加载。

## 本次验证结果

- 官方 `com.obsproject.Studio` 版本：`32.2.1`。
- 插件 ref：`runtime/com.obsproject.Studio.Plugin.MultiRTMP/x86_64/stable`。
- 插件扩展 commit：
  `bae08da49d950dcb900ae5d0d353e558531bbb80c1078eb9994d5c0b4a970bba`。
- bundle：`release/obs-multi-rtmp.flatpak`，约 3.1 MB；安装后约 13.6 MB。
- bundle SHA-256：
  `ACFEA2C23D1392E99D8C723D9F254246104FBBCD3CAA4EC7289C728F32DB908C`。
- `appstreamcli validate --no-net` 通过。
- 沙箱内 `ldd` 检查无缺失依赖，libobs、obs-frontend-api 和 Qt6 均从官方 OBS
  runtime 的 `/app/lib` 解析。
- 限时启动 OBS 后，日志列出 `com.obsproject.Studio.Plugin.MultiRTMP`，加载
  `obs-multi-rtmp.so`，报告插件版本 `0.7.4.0`，并正常到达
  `Startup complete`。

## 发布时需要调整的内容

当前清单使用 `type: dir` 读取工作区，适合本地开发和生成 bundle。提交 Flathub
时应把 source 改为固定 `tag` 与 `commit` 的 Git source，并把
`flatpak/com.obsproject.Studio.Plugin.MultiRTMP.metainfo.xml` 的 release 版本和日期
同步到正式发布版本。Flathub 通常还需要一个独立打包仓库及 `flathub.json`；其中
`only-arches` 应与实际测试过的架构一致。
