# 博客XIU

XIU 的个人博客，纯静态网站（HTML + CSS + 原生 JS），无需构建。

## 本地预览

直接用浏览器打开 `index.html`，或启动一个静态服务器：

```bash
python3 -m http.server 8000
# 访问 http://localhost:8000
```

## 修改内容

- `index.html`：简介、文章列表、联系方式
- `style.css`：配色在 `:root` 的 CSS 变量里
- `script.js`：深色/浅色模式切换

## 写新文章

1. 复制 `posts/hello-world.html` 为 `posts/新文章名.html`，修改标题、日期和正文
2. 在 `index.html` 的 `#posts` 列表最上面加一条 `<li>` 指向它

## 部署

推到 GitHub 后，在仓库 Settings → Pages 选择分支即可用 GitHub Pages 发布。
