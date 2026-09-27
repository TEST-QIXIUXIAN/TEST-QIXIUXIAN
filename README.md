# 个人主页

纯静态个人主页（HTML + CSS + 原生 JS），无需构建。

## 本地预览

直接用浏览器打开 `index.html`，或启动一个静态服务器：

```bash
python3 -m http.server 8000
# 访问 http://localhost:8000
```

## 修改内容

- `index.html`：姓名、简介、项目、联系方式（搜索"你的名字"替换）
- `style.css`：配色在 `:root` 的 CSS 变量里
- `script.js`：深色/浅色模式切换

## 部署

推到 GitHub 后，在仓库 Settings → Pages 选择分支即可用 GitHub Pages 发布。
