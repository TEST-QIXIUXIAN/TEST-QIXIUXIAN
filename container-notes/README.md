# 容器命令速记（container-notes）

一个用 C 语言写的小工具，专门用来收集和查找容器相关的命令与代码片段（Docker、Dockerfile、Compose、Kubernetes、Helm 等）。程序运行后会在本机启动一个网页，在浏览器里使用。

- 单个 C 文件，不依赖第三方库，编译出来就是一个 `container-notes.exe`
- 双击启动后自动打开浏览器，地址是 `http://127.0.0.1:8080/`；8080 被占用时会自动换用下一个端口
- 功能：按分类浏览、多关键词搜索（命中处高亮）、一键复制命令、新增 / 编辑 / 删除，导出 Markdown 或 JSON，从 JSON 导入（重复的条目自动跳过）
- 内置 650 多条常用命令和模板，覆盖 Docker（容器、镜像、Buildx）、Dockerfile、Compose、Kubernetes、K8s YAML 模板、Helm、OpenShift、Podman、containerd / crictl、镜像仓库、网络与存储、排障调试、Windows 与 WSL。第一次运行时自动写入，可以随意修改或删除
- OpenShift 分三类：「OpenShift」（日常开发场景）、「OpenShift 管理」（集群运维场景）、「OpenShift 命令大全」（`oc` 的全部 226 个官方命令，示例取自官方 `oc --help`）
- **在线自动更新**：页面每天最多检查一次在线命令库（本仓库里的 `seed.json`），发现新命令会自动加进来；你删掉的命令不会被加回来。网络不通时什么也不做，下次再试。也可以随时点「更新命令库」手动检查
- 数据保存在 exe 旁边的 `container-notes.db`（UTF-8 纯文本），备份时复制这个文件即可
- 只监听本机 `127.0.0.1`，局域网里的其他电脑访问不到，一般也不会触发防火墙提示

## 在 Windows 11 上编译

需要一个 C 编译器，下面两种任选一个：

1. **MinGW-w64（gcc）**：比如安装 [MSYS2](https://www.msys2.org/) 后执行 `pacman -S mingw-w64-ucrt-x86_64-gcc`，再把 `C:\msys64\ucrt64\bin` 加到 PATH。
2. **Visual Studio Build Tools（cl）**：安装时勾选“使用 C++ 的桌面开发”，然后在“Developer Command Prompt for VS”里操作。

然后双击 `build.bat`，或者手动执行：

```bat
:: MinGW-w64
gcc -O2 -static -o container-notes.exe container_notes.c -lws2_32 -lshell32

:: MSVC
cl /nologo /O2 /utf-8 container_notes.c ws2_32.lib shell32.lib /Fe:container-notes.exe
```

## 使用

双击 `container-notes.exe`，会弹出一个控制台窗口，同时打开浏览器。**使用期间不要关闭这个窗口**，关掉窗口就退出了。

- 按 `/` 快速聚焦搜索框；多个关键词用空格隔开，比如 `k8s 日志` 或 `logs pod`
- 在编辑框里按 `Ctrl + Enter` 保存
- 想开机自启：把 exe 的快捷方式放进 `shell:startup` 文件夹

命令行参数：

```
container-notes.exe [-p 端口] [--db 数据文件] [--no-browser]
```

## 命令库如何保持最新

`tools/` 里的脚本从官方 `oc` 程序的帮助信息里列出全部命令：

```bash
python3 tools/oc_commands.py /path/to/oc > tools/oc-commands.json   # 列出官方全部 oc 命令
python3 tools/sync_oc.py tools/oc-commands.json                     # 把新命令加入 seed.json
python3 gen_page.py                                                 # 重新生成 page.h
```

新命令的中文标题写在 `tools/oc_zh.json`。云端有一个每周运行的定时任务：从 github.com/openshift/oc 最新的 release 分支编译 `oc`，执行上面三步，有新命令时提交到仓库。这些新命令会被已经在运行的程序自动拉取，不需要重新编译 exe。

## 修改界面

界面在 `index.html`，预置数据在 `seed.json`。修改后执行 `python gen_page.py` 重新生成 `page.h`，再重新编译即可。`page.h` 里所有中文都转成了转义字符，所以无论 Windows 用的是什么代码页，编译结果都一样。

Linux / macOS 下也能编译：`cc -O2 -pthread -o container-notes container_notes.c`
