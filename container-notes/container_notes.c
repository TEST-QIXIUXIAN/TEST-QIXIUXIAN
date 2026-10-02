/*
 * container-notes - a small local web app for collecting container-related
 * commands and code snippets (Docker, Compose, Kubernetes, Helm, ...).
 *
 * One C file, no third-party libraries. It runs an HTTP server bound to
 * 127.0.0.1, serves the UI embedded from page.h and keeps the data in
 * container-notes.db next to the executable.
 *
 * Windows 11 (MinGW-w64): gcc -O2 -static -o container-notes.exe container_notes.c -lws2_32 -lshell32
 * Windows 11 (MSVC):      cl /nologo /O2 /utf-8 container_notes.c ws2_32.lib /Fe:container-notes.exe
 * Linux / macOS:          cc -O2 -pthread -o container-notes container_notes.c
 *
 * This file is kept ASCII-only so it compiles the same under any code page;
 * all Chinese UI text lives in page.h, which gen_page.py generates from
 * index.html and seed.json with every non-ASCII byte written as an escape.
 */
#ifdef _WIN32
#  if defined(__MINGW32__) && !defined(__USE_MINGW_ANSI_STDIO)
#    define __USE_MINGW_ANSI_STDIO 1
#  endif
#  ifndef _CRT_SECURE_NO_WARNINGS
#    define _CRT_SECURE_NO_WARNINGS
#  endif
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <winsock2.h>
#  include <ws2tcpip.h>
#  include <windows.h>
#  include <shellapi.h>
#  ifdef _MSC_VER
#    pragma comment(lib, "ws2_32.lib")
#    pragma comment(lib, "shell32.lib")
#  endif
typedef SOCKET sock_t;
#  define close_socket closesocket
#  define SHUT_WR SD_SEND
#else
#  include <arpa/inet.h>
#  include <netinet/in.h>
#  include <pthread.h>
#  include <signal.h>
#  include <sys/socket.h>
#  include <sys/time.h>
#  include <unistd.h>
typedef int sock_t;
#  define INVALID_SOCKET (-1)
#  define close_socket close
#endif

#include <ctype.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "page.h"

#define APP_NAME     "container-notes"
#define DB_FILE      "container-notes.db"
#define DEFAULT_PORT 8080
#define MAX_HEADER   (16 * 1024)
#define MAX_BODY     (4 * 1024 * 1024)

/* "其他" (Other) in UTF-8, the fallback category. */
#define CAT_OTHER "\345\205\266\344\273\226"

/* ------------------------------------------------------------------ lock */

#ifdef _WIN32
static CRITICAL_SECTION g_lock;
static void db_lock_init(void) { InitializeCriticalSection(&g_lock); }
static void db_lock(void) { EnterCriticalSection(&g_lock); }
static void db_unlock(void) { LeaveCriticalSection(&g_lock); }
#else
static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;
static void db_lock_init(void) {}
static void db_lock(void) { pthread_mutex_lock(&g_lock); }
static void db_unlock(void) { pthread_mutex_unlock(&g_lock); }
#endif

/* ---------------------------------------------------------------- buffer */

typedef struct {
    char *p;
    size_t n, cap;
} Buf;

static void die_oom(void)
{
    fputs("out of memory\n", stderr);
    exit(1);
}

static void buf_add(Buf *b, const char *s, size_t len)
{
    if (b->n + len + 1 > b->cap) {
        size_t cap = b->cap ? b->cap : 1024;
        char *np;
        while (cap < b->n + len + 1)
            cap *= 2;
        np = (char *)realloc(b->p, cap);
        if (!np)
            die_oom();
        b->p = np;
        b->cap = cap;
    }
    memcpy(b->p + b->n, s, len);
    b->n += len;
    b->p[b->n] = '\0';
}

static void buf_puts(Buf *b, const char *s) { buf_add(b, s, strlen(s)); }

static void buf_printf(Buf *b, const char *fmt, ...)
{
    char tmp[256];
    int k;
    va_list ap;
    va_start(ap, fmt);
    k = vsnprintf(tmp, sizeof tmp, fmt, ap);
    va_end(ap);
    if (k > 0)
        buf_add(b, tmp, (size_t)k < sizeof tmp ? (size_t)k : sizeof tmp - 1);
}

static void buf_json(Buf *b, const char *s)
{
    const unsigned char *p;
    buf_add(b, "\"", 1);
    for (p = (const unsigned char *)s; *p; p++) {
        switch (*p) {
        case '"': buf_add(b, "\\\"", 2); break;
        case '\\': buf_add(b, "\\\\", 2); break;
        case '\n': buf_add(b, "\\n", 2); break;
        case '\r': buf_add(b, "\\r", 2); break;
        case '\t': buf_add(b, "\\t", 2); break;
        default:
            if (*p < 0x20)
                buf_printf(b, "\\u%04x", *p);
            else
                buf_add(b, (const char *)p, 1);
        }
    }
    buf_add(b, "\"", 1);
}

static char *xstrdup(const char *s)
{
    size_t n = strlen(s) + 1;
    char *p = (char *)malloc(n);
    if (!p)
        die_oom();
    memcpy(p, s, n);
    return p;
}

/* ----------------------------------------------------------------- store */

typedef struct {
    int id;
    char *cat, *title, *code, *note;
    long long ts; /* last update, unix seconds; 0 for built-in examples */
} Snip;

static Snip *g_items;
static int g_count, g_cap, g_next_id = 1;
static char g_db_path[4096];
static int g_port = DEFAULT_PORT;

static int item_find(int id)
{
    int i;
    for (i = 0; i < g_count; i++)
        if (g_items[i].id == id)
            return i;
    return -1;
}

static Snip *item_add(int id)
{
    Snip *s;
    if (g_count == g_cap) {
        int cap = g_cap ? g_cap * 2 : 64;
        Snip *np = (Snip *)realloc(g_items, (size_t)cap * sizeof *np);
        if (!np)
            die_oom();
        g_items = np;
        g_cap = cap;
    }
    s = &g_items[g_count++];
    memset(s, 0, sizeof *s);
    s->id = id;
    if (id >= g_next_id)
        g_next_id = id + 1;
    return s;
}

static void item_set(Snip *s, const char *cat, const char *title, const char *code, const char *note)
{
    free(s->cat);
    free(s->title);
    free(s->code);
    free(s->note);
    s->cat = xstrdup(cat);
    s->title = xstrdup(title);
    s->code = xstrdup(code);
    s->note = xstrdup(note);
}

static void item_remove(int idx)
{
    Snip *s = &g_items[idx];
    free(s->cat);
    free(s->title);
    free(s->code);
    free(s->note);
    memmove(s, s + 1, (size_t)(g_count - idx - 1) * sizeof *s);
    g_count--;
}

/* Data file: one record per line, tab separated:
 *   id \t category \t title \t code \t note \t updated
 * with backslash, tab, CR and LF inside fields escaped as \\ \t \r \n.
 * Lines that do not start with a positive id (e.g. '#' comments) are ignored. */

static void db_unescape(char *s)
{
    char *r, *w = s;
    for (r = s; *r; r++) {
        if (*r == '\\' && r[1]) {
            r++;
            *w++ = *r == 'n' ? '\n' : *r == 't' ? '\t' : *r == 'r' ? '\r' : *r;
        } else {
            *w++ = *r;
        }
    }
    *w = '\0';
}

static void db_parse(char *text)
{
    char *line = text;
    while (line && *line) {
        char *nl = strchr(line, '\n');
        char *f[6] = {0};
        int nf = 0, i, id;
        size_t len;
        char *p = line;

        if (nl)
            *nl = '\0';
        len = strlen(line);
        if (len && line[len - 1] == '\r')
            line[len - 1] = '\0';

        while (nf < 6) {
            char *t;
            f[nf++] = p;
            t = strchr(p, '\t');
            if (!t)
                break;
            *t = '\0';
            p = t + 1;
        }
        id = atoi(f[0]);
        if (nf >= 4 && id > 0 && item_find(id) < 0) {
            Snip *s;
            for (i = 1; i < nf; i++)
                db_unescape(f[i]);
            s = item_add(id);
            item_set(s, *f[1] ? f[1] : CAT_OTHER, f[2], f[3], nf > 4 ? f[4] : "");
            s->ts = nf > 5 ? atoll(f[5]) : 0;
        }
        line = nl ? nl + 1 : NULL;
    }
}

static void db_put(FILE *f, const char *s)
{
    for (; *s; s++) {
        switch (*s) {
        case '\\': fputs("\\\\", f); break;
        case '\t': fputs("\\t", f); break;
        case '\n': fputs("\\n", f); break;
        case '\r': fputs("\\r", f); break;
        default: fputc(*s, f);
        }
    }
}

static int db_save(void)
{
    char tmp[sizeof g_db_path + 8];
    FILE *f;
    int i, bad;

    snprintf(tmp, sizeof tmp, "%s.tmp", g_db_path);
    f = fopen(tmp, "wb");
    if (!f)
        return -1;
    fputs("# container-notes data (tab separated; edit only while the program is stopped)\n", f);
    for (i = 0; i < g_count; i++) {
        const Snip *s = &g_items[i];
        fprintf(f, "%d\t", s->id);
        db_put(f, s->cat);
        fputc('\t', f);
        db_put(f, s->title);
        fputc('\t', f);
        db_put(f, s->code);
        fputc('\t', f);
        db_put(f, s->note);
        fprintf(f, "\t%lld\n", s->ts);
    }
    bad = ferror(f);
    if (fclose(f) != 0 || bad) {
        remove(tmp);
        return -1;
    }
#ifdef _WIN32
    if (!MoveFileExA(tmp, g_db_path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        return -1;
#else
    if (rename(tmp, g_db_path) != 0)
        return -1;
#endif
    return 0;
}

/* Loads the data file; on first run seeds it with the built-in examples. */
static int db_load(void)
{
    FILE *f = fopen(g_db_path, "rb");
    Buf b = {0};
    char chunk[8192];
    size_t k;

    if (!f) {
        int i;
        for (i = 0; SEED_DB_PARTS[i]; i++)
            buf_puts(&b, SEED_DB_PARTS[i]);
        db_parse(b.p);
        free(b.p);
        return db_save();
    }
    while ((k = fread(chunk, 1, sizeof chunk, f)) > 0)
        buf_add(&b, chunk, k);
    fclose(f);
    if (b.p) {
        /* skip a UTF-8 BOM in case the file was saved by Notepad */
        db_parse(strncmp(b.p, "\xEF\xBB\xBF", 3) == 0 ? b.p + 3 : b.p);
        free(b.p);
    }
    return 0;
}

/* ------------------------------------------------------------------ http */

static int send_all(sock_t s, const char *p, size_t n)
{
    while (n) {
        int k = send(s, p, n > 65536 ? 65536 : (int)n, 0);
        if (k <= 0)
            return -1;
        p += k;
        n -= (size_t)k;
    }
    return 0;
}

static const char *status_text(int code)
{
    switch (code) {
    case 200: return "OK";
    case 400: return "Bad Request";
    case 403: return "Forbidden";
    case 404: return "Not Found";
    case 405: return "Method Not Allowed";
    case 413: return "Payload Too Large";
    case 431: return "Request Header Fields Too Large";
    default: return "Internal Server Error";
    }
}

static void send_head(sock_t s, int code, const char *ctype, size_t len, const char *extra)
{
    char h[1024];
    int k = snprintf(h, sizeof h,
                     "HTTP/1.1 %d %s\r\n"
                     "Content-Type: %s\r\n"
                     "Content-Length: %lu\r\n"
                     "Cache-Control: no-store\r\n"
                     "X-Content-Type-Options: nosniff\r\n"
                     "Connection: close\r\n"
                     "%s\r\n",
                     code, status_text(code), ctype, (unsigned long)len, extra ? extra : "");
    if (k > 0)
        send_all(s, h, (size_t)k < sizeof h ? (size_t)k : sizeof h - 1);
}

static void send_json(sock_t s, int code, const Buf *b)
{
    send_head(s, code, "application/json; charset=utf-8", b->n, NULL);
    send_all(s, b->p, b->n);
}

static void send_error(sock_t s, int code, const char *msg)
{
    Buf b = {0};
    buf_puts(&b, "{\"ok\":false,\"error\":");
    buf_json(&b, msg);
    buf_puts(&b, "}");
    send_json(s, code, &b);
    free(b.p);
}

/* Sends a NULL-terminated array of string chunks from page.h as one response. */
static void send_parts(sock_t s, const char *const *parts, const char *ctype, const char *extra, int head_only)
{
    size_t total = 0;
    int i;
    for (i = 0; parts[i]; i++)
        total += strlen(parts[i]);
    send_head(s, 200, ctype, total, extra);
    if (head_only)
        return;
    for (i = 0; parts[i]; i++)
        if (send_all(s, parts[i], strlen(parts[i])) != 0)
            return;
}

static void send_page(sock_t s, int head_only)
{
    send_parts(s, PAGE_PARTS, "text/html; charset=utf-8",
               "Content-Security-Policy: default-src 'self'; script-src 'unsafe-inline'; "
               "style-src 'unsafe-inline'; img-src 'self' data:; "
               "connect-src 'self' https://raw.githubusercontent.com\r\n"
               "Referrer-Policy: no-referrer\r\n",
               head_only);
}

static int ieq_n(const char *a, const char *b, size_t n)
{
    size_t i;
    for (i = 0; i < n; i++)
        if (tolower((unsigned char)a[i]) != tolower((unsigned char)b[i]))
            return 0;
    return 1;
}

static int ieq(const char *a, const char *b)
{
    size_t n = strlen(a);
    return n == strlen(b) && ieq_n(a, b, n);
}

/* Finds header `name` in a NUL-terminated header block (request line first). */
static int get_header(const char *head, const char *name, char *out, size_t outsz)
{
    size_t nl = strlen(name);
    const char *p = strstr(head, "\r\n");
    while (p) {
        const char *e;
        p += 2;
        e = strstr(p, "\r\n");
        if (!e || e == p)
            break;
        if ((size_t)(e - p) > nl && p[nl] == ':' && ieq_n(p, name, nl)) {
            const char *v = p + nl + 1;
            size_t vl;
            while (*v == ' ' || *v == '\t')
                v++;
            vl = (size_t)(e - v);
            while (vl && (v[vl - 1] == ' ' || v[vl - 1] == '\t'))
                vl--;
            if (vl >= outsz)
                vl = outsz - 1;
            memcpy(out, v, vl);
            out[vl] = '\0';
            return 1;
        }
        p = e;
    }
    return 0;
}

/* Only answer to our own origin; blocks DNS-rebinding pages from reaching the API. */
static int host_ok(const char *host)
{
    char a[64], b[64];
    snprintf(a, sizeof a, "127.0.0.1:%d", g_port);
    snprintf(b, sizeof b, "localhost:%d", g_port);
    if (ieq(host, a) || ieq(host, b))
        return 1;
    return g_port == 80 && (ieq(host, "127.0.0.1") || ieq(host, "localhost"));
}

static int hexval(int c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static char *url_decode(const char *s, size_t n)
{
    char *o = (char *)malloc(n + 1);
    size_t i, j = 0;
    if (!o)
        die_oom();
    for (i = 0; i < n; i++) {
        if (s[i] == '+') {
            o[j++] = ' ';
        } else if (s[i] == '%' && i + 2 < n && hexval(s[i + 1]) >= 0 && hexval(s[i + 2]) >= 0) {
            o[j++] = (char)(hexval(s[i + 1]) * 16 + hexval(s[i + 2]));
            i += 2;
        } else {
            o[j++] = s[i];
        }
    }
    o[j] = '\0';
    return o;
}

/* Returns the decoded value of `key` in an x-www-form-urlencoded body, or NULL. */
static char *form_get(const char *body, const char *key)
{
    const char *p = body;
    while (*p) {
        const char *amp = strchr(p, '&');
        size_t len = amp ? (size_t)(amp - p) : strlen(p);
        const char *eq = (const char *)memchr(p, '=', len);
        size_t klen = eq ? (size_t)(eq - p) : len;
        char *k = url_decode(p, klen);
        int match = strcmp(k, key) == 0;
        free(k);
        if (match)
            return eq ? url_decode(eq + 1, len - klen - 1) : xstrdup("");
        if (!amp)
            break;
        p = amp + 1;
    }
    return NULL;
}

/* ------------------------------------------------------------------- api */

static void api_list(sock_t c)
{
    Buf b = {0};
    int i;
    buf_puts(&b, "{\"ok\":true,\"items\":[");
    db_lock();
    for (i = 0; i < g_count; i++) {
        const Snip *s = &g_items[i];
        buf_printf(&b, "%s{\"id\":%d,\"category\":", i ? "," : "", s->id);
        buf_json(&b, s->cat);
        buf_puts(&b, ",\"title\":");
        buf_json(&b, s->title);
        buf_puts(&b, ",\"code\":");
        buf_json(&b, s->code);
        buf_puts(&b, ",\"note\":");
        buf_json(&b, s->note);
        buf_printf(&b, ",\"updated\":%lld}", s->ts);
    }
    db_unlock();
    buf_puts(&b, "]}");
    send_json(c, 200, &b);
    free(b.p);
}

static void api_save(sock_t c, const char *body)
{
    char *id_s = form_get(body, "id");
    char *cat = form_get(body, "category");
    char *title = form_get(body, "title");
    char *code = form_get(body, "code");
    char *note = form_get(body, "note");
    int id = id_s ? atoi(id_s) : 0;
    Snip *s;
    int rc;
    Buf b = {0};

    if ((!title || !*title) && (!code || !*code)) {
        send_error(c, 400, "title or code is required");
        goto out;
    }
    db_lock();
    if (id > 0) {
        int idx = item_find(id);
        if (idx < 0) {
            db_unlock();
            send_error(c, 404, "record not found (it may have been deleted)");
            goto out;
        }
        s = &g_items[idx];
    } else {
        s = item_add(g_next_id);
    }
    item_set(s, cat && *cat ? cat : CAT_OTHER, title ? title : "", code ? code : "", note ? note : "");
    s->ts = (long long)time(NULL);
    id = s->id;
    rc = db_save();
    db_unlock();
    if (rc != 0) {
        send_error(c, 500, "failed to write the data file");
        goto out;
    }
    buf_printf(&b, "{\"ok\":true,\"id\":%d}", id);
    send_json(c, 200, &b);
out:
    free(b.p);
    free(id_s);
    free(cat);
    free(title);
    free(code);
    free(note);
}

static void api_delete(sock_t c, const char *body)
{
    char *id_s = form_get(body, "id");
    int idx, rc;
    Buf b = {0};

    db_lock();
    idx = id_s ? item_find(atoi(id_s)) : -1;
    if (idx < 0) {
        db_unlock();
        send_error(c, 404, "record not found");
        free(id_s);
        return;
    }
    item_remove(idx);
    rc = db_save();
    db_unlock();
    free(id_s);
    if (rc != 0) {
        send_error(c, 500, "failed to write the data file");
        return;
    }
    buf_puts(&b, "{\"ok\":true}");
    send_json(c, 200, &b);
    free(b.p);
}

/* ------------------------------------------------------------ connection */

static void handle(sock_t c)
{
    char *req = (char *)malloc(MAX_HEADER + 1);
    char *body = NULL, *hend = NULL, *qm;
    char method[8], target[2048], host[256] = "", tmp[64];
    size_t got = 0, head_len, have;

    if (!req)
        goto done;
    while (got < MAX_HEADER) {
        int k = recv(c, req + got, (int)(MAX_HEADER - got), 0);
        if (k <= 0)
            goto done;
        got += (size_t)k;
        req[got] = '\0';
        if ((hend = strstr(req, "\r\n\r\n")) != NULL)
            break;
    }
    if (!hend) {
        send_error(c, 431, "request header too large");
        goto done;
    }
    if (sscanf(req, "%7s %2047s", method, target) != 2) {
        send_error(c, 400, "bad request");
        goto done;
    }
    head_len = (size_t)(hend - req) + 4;
    have = got - head_len;
    hend[2] = '\0'; /* header block now ends with the last header's CRLF */

    get_header(req, "Host", host, sizeof host);
    if (!host_ok(host)) {
        send_error(c, 403, "open this app via http://127.0.0.1 only");
        goto done;
    }
    qm = strchr(target, '?');
    if (qm)
        *qm = '\0';

    if (!strcmp(method, "GET") || !strcmp(method, "HEAD")) {
        if (!strcmp(target, "/") || !strcmp(target, "/index.html"))
            send_page(c, method[0] == 'H');
        else if (!strcmp(target, "/api/list"))
            api_list(c);
        else if (!strcmp(target, "/api/seed")) /* the built-in commands, for merging into old data */
            send_parts(c, SEED_JSON_PARTS, "application/json; charset=utf-8", NULL, 0);
        else
            send_error(c, 404, "not found");
    } else if (!strcmp(method, "POST")) {
        char xrw[64] = "";
        size_t clen = 0, bn;
        /* a custom header cannot be sent cross-site without a CORS preflight,
         * which we never approve - so other web pages cannot change the data */
        get_header(req, "X-Requested-With", xrw, sizeof xrw);
        if (strcmp(xrw, APP_NAME) != 0) {
            send_error(c, 403, "forbidden");
            goto done;
        }
        if (get_header(req, "Content-Length", tmp, sizeof tmp))
            clen = (size_t)strtoul(tmp, NULL, 10);
        if (clen > MAX_BODY) {
            send_error(c, 413, "content too large (limit 4 MB)");
            goto done;
        }
        body = (char *)malloc(clen + 1);
        if (!body)
            goto done;
        bn = have < clen ? have : clen;
        memcpy(body, req + head_len, bn);
        while (bn < clen) {
            int k = recv(c, body + bn, (int)(clen - bn), 0);
            if (k <= 0)
                goto done;
            bn += (size_t)k;
        }
        body[clen] = '\0';
        if (!strcmp(target, "/api/save"))
            api_save(c, body);
        else if (!strcmp(target, "/api/delete"))
            api_delete(c, body);
        else
            send_error(c, 404, "not found");
    } else {
        send_error(c, 405, "method not allowed");
    }
done:
    free(req);
    free(body);
    shutdown(c, SHUT_WR);
    close_socket(c);
}

#ifdef _WIN32
static DWORD WINAPI conn_thread(LPVOID arg)
{
    handle((sock_t)(uintptr_t)arg);
    return 0;
}
#else
static void *conn_thread(void *arg)
{
    handle((sock_t)(intptr_t)arg);
    return NULL;
}
#endif

static void spawn(sock_t c)
{
#ifdef _WIN32
    HANDLE h = CreateThread(NULL, 0, conn_thread, (LPVOID)(uintptr_t)c, 0, NULL);
    if (h)
        CloseHandle(h);
    else
        handle(c);
#else
    pthread_t t;
    if (pthread_create(&t, NULL, conn_thread, (void *)(intptr_t)c) == 0)
        pthread_detach(t);
    else
        handle(c);
#endif
}

static void set_timeouts(sock_t c)
{
#ifdef _WIN32
    DWORD ms = 15000;
    setsockopt(c, SOL_SOCKET, SO_RCVTIMEO, (const char *)&ms, sizeof ms);
    setsockopt(c, SOL_SOCKET, SO_SNDTIMEO, (const char *)&ms, sizeof ms);
#else
    struct timeval tv;
    tv.tv_sec = 15;
    tv.tv_usec = 0;
    setsockopt(c, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
    setsockopt(c, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof tv);
#endif
}

static sock_t try_listen(int port)
{
    struct sockaddr_in a;
    sock_t s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET)
        return s;
#ifdef _WIN32
    {
        BOOL on = TRUE;
        setsockopt(s, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, (const char *)&on, sizeof on);
    }
#else
    {
        int on = 1;
        setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &on, sizeof on);
    }
#endif
    memset(&a, 0, sizeof a);
    a.sin_family = AF_INET;
    a.sin_port = htons((unsigned short)port);
    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (bind(s, (struct sockaddr *)&a, sizeof a) != 0 || listen(s, 64) != 0) {
        close_socket(s);
        return INVALID_SOCKET;
    }
    return s;
}

static void init_db_path(void)
{
#ifdef _WIN32
    /* keep the data beside the .exe so double-clicking always finds it */
    char exe[MAX_PATH];
    DWORD n = GetModuleFileNameA(NULL, exe, MAX_PATH);
    if (n > 0 && n < MAX_PATH) {
        char *sl = strrchr(exe, '\\');
        if (sl) {
            sl[1] = '\0';
            snprintf(g_db_path, sizeof g_db_path, "%s%s", exe, DB_FILE);
            return;
        }
    }
#endif
    snprintf(g_db_path, sizeof g_db_path, "%s", DB_FILE);
}

static int fail(const char *msg)
{
    fprintf(stderr, "error: %s\n", msg);
#ifdef _WIN32
    /* keep the console open when started by double-click */
    fputs("Press Enter to exit...", stderr);
    getchar();
#endif
    return 1;
}

static void usage(void)
{
    puts("usage: " APP_NAME " [-p PORT] [--db FILE] [--no-browser]\n"
         "  -p, --port PORT  port to listen on (default 8080; the next free one is used if busy)\n"
         "  --db FILE        data file (default: " DB_FILE " beside the program)\n"
         "  --no-browser     do not open the browser on start");
}

int main(int argc, char **argv)
{
    int port = DEFAULT_PORT, open_browser = 1, i, p;
    const char *db = NULL;
    sock_t srv = INVALID_SOCKET;

    for (i = 1; i < argc; i++) {
        if ((!strcmp(argv[i], "-p") || !strcmp(argv[i], "--port")) && i + 1 < argc) {
            port = atoi(argv[++i]);
        } else if (!strcmp(argv[i], "--db") && i + 1 < argc) {
            db = argv[++i];
        } else if (!strcmp(argv[i], "--no-browser")) {
            open_browser = 0;
        } else {
            usage();
            return strcmp(argv[i], "-h") && strcmp(argv[i], "--help") ? 2 : 0;
        }
    }
    if (port <= 0 || port > 65535)
        return fail("invalid port");

#ifdef _WIN32
    {
        WSADATA w;
        if (WSAStartup(MAKEWORD(2, 2), &w) != 0)
            return fail("WSAStartup failed");
        SetConsoleTitleA(APP_NAME);
    }
#else
    signal(SIGPIPE, SIG_IGN);
#endif

    if (db)
        snprintf(g_db_path, sizeof g_db_path, "%s", db);
    else
        init_db_path();
    db_lock_init();
    if (db_load() != 0)
        fprintf(stderr, "warning: cannot write %s - changes will not be saved\n", g_db_path);

    for (p = port; p < port + 10 && p <= 65535; p++) {
        srv = try_listen(p);
        if (srv != INVALID_SOCKET) {
            g_port = p;
            break;
        }
    }
    if (srv == INVALID_SOCKET)
        return fail("no free port found (tried 10 ports from the one given); use -p to pick another");

    printf("container-notes is running:  http://127.0.0.1:%d/\n", g_port);
    printf("data file: %s  (%d records)\n", g_db_path, g_count);
    printf("keep this window open while using the page; press Ctrl+C to stop.\n");
    fflush(stdout);

#ifdef _WIN32
    if (open_browser) {
        char url[64];
        snprintf(url, sizeof url, "http://127.0.0.1:%d/", g_port);
        ShellExecuteA(NULL, "open", url, NULL, NULL, SW_SHOWNORMAL);
    }
#else
    (void)open_browser;
#endif

    for (;;) {
        sock_t c = accept(srv, NULL, NULL);
        if (c == INVALID_SOCKET)
            continue;
        set_timeouts(c);
        spawn(c);
    }
}
