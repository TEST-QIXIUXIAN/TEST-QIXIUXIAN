# 博客XIU

灵感与感悟的诺尔曼 —— 纯静态博客（HTML + CSS + 原生 JS）。

线上地址：https://test-qixiuxian.github.io/TEST-QIXIUXIAN/

## 本地预览

直接用浏览器打开 `index.html`，或启动一个静态服务器：

```bash
python3 -m http.server 8000
# 访问 http://localhost:8000
```

## 用 Docker 运行

```bash
docker compose up -d --build
# 访问 http://localhost:8080
```

或者不用 compose：

```bash
docker build -t blog-xiu .
docker run -d -p 8080:80 blog-xiu
```

镜像构建时会自动运行 `build.py`，再用 nginx 提供静态文件。容器默认监听 80 端口，设置环境变量 `PORT` 可以改端口（Render、Railway 会自动设置）。

## 部署到 Render

仓库里的 `render.yaml` 已经配置好（Docker、免费套餐、推送后自动部署）：

1. 用 GitHub 账号登录 https://dashboard.render.com
2. 点 **New → Blueprint**，选择这个仓库，确认后点 **Apply**
3. 几分钟后得到 `https://blog-xiu-xxxx.onrender.com` 这样的网址

免费套餐 15 分钟没人访问会休眠，下一次打开需要等几十秒唤醒。

## 修改内容

- `index.html`：简介、文章列表、联系方式
- `style.css`：配色在 `:root` 的 CSS 变量里
- `script.js`：深色/浅色模式切换

## 写新文章

最快的方式是用脚本一键生成：

```bash
python3 new_post.py "文章标题" -t 灵感,随笔 -d "一句话摘要"
```

它会从模板生成 `posts/<日期>.html`（可用 `-s 英文名` 指定文件名，`--date` 指定日期），并自动运行 `build.py`。之后编辑正文即可；改了标题、摘要或分类，再运行一次 `python3 build.py`。

也可以手动：

1. 复制 `posts/hello-world.html` 为 `posts/新文章名.html`
2. 修改 `<head>` 里的 `description`（摘要）、`date`（日期）、`tags`（分类，逗号分隔），以及 `<h1>` 标题和正文
3. 运行 `python3 build.py`，会自动更新首页文章列表、分类筛选按钮和 RSS（`feed.xml`）

## 部署

网站由 GitHub Pages 从 `claude/hi-vxtgib` 分支根目录发布，推送后 1 分钟左右自动更新。
