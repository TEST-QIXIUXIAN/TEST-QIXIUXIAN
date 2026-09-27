# Build stage: regenerate post list / feed from posts/*.html so the image
# never ships stale output even if build.py wasn't run before committing.
FROM python:3.12-alpine AS build
WORKDIR /site
COPY . .
RUN python3 build.py

# Serve stage: plain static files behind nginx.
FROM nginx:1.27-alpine
COPY --from=build /site/index.html /site/style.css /site/script.js /site/feed.xml \
     /site/favicon.svg /site/apple-touch-icon.png /site/og.png /usr/share/nginx/html/
COPY --from=build /site/posts /usr/share/nginx/html/posts
EXPOSE 80
