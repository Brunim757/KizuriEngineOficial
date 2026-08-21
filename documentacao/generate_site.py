#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Gerador do site de documentação da Kizuri Engine — v2.

Lê Markdown de src/ (com front matter) e gera um site estático profissional
em site/: sidebar fixa com busca, breadcrumb, prev/next, tema claro/escuro,
botão "copiar código", destaque de sintaxe, callouts (dica/nota/aviso) e uma
página de busca com índice client-side. Só biblioteca padrão, saída 100% HTML
estático — hospedável em qualquer lugar (Vercel, GitHub Pages, nginx, file://).

Front matter de cada página:
    ---
    title:   Título na sidebar
    group:   Seção (Introdução, Editor, Componentes, Scripting C#, Gráficos, Distribuição)
    order:   posição dentro da seção (1, 2, 3...)
    ---

Uso:  python3 generate_site.py
"""
import os
import re
import sys
import json
import html

BASE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(BASE, "src")
OUT = os.path.join(BASE, "site")

SITE_TITLE = "Kizuri Engine"
SITE_SUBTITLE = "Documentação"
VERSION = "0.39.3"
GROUPS = ["Introdução", "Editor", "Componentes", "Mundo / Streaming", "Scripting C#", "Gráficos",
          "Distribuição"]

# ---------------------------------------------------------------------------
# Markdown -> HTML (leve, sem dependências)
# ---------------------------------------------------------------------------

def esc(s: str) -> str:
    return html.escape(s, quote=False)

def render_inline(s: str) -> str:
    """Código inline, links, negrito e itálico — sem tocar em código."""
    parts, pos = [], 0
    for m in re.finditer(r"`([^`]+)`", s):
        parts.append((s[pos:m.start()], True))
        parts.append((m.group(1), False))
        pos = m.end()
    parts.append((s[pos:], True))
    out = []
    for text, is_code in parts:
        if not is_code:
            out.append("<code>" + esc(text) + "</code>")
            continue
        t = re.sub(r"\[([^\]]+)\]\(([^)]+)\)", r'<a href="\2">\1</a>', text)
        t = re.sub(r"\*\*([^*]+)\*\*", r"<strong>\1</strong>", t)
        t = re.sub(r"(?<!\*)\*([^*]+)\*(?!\*)", r"<em>\1</em>", t)
        t = re.sub(r"(?<!\!)\!\[([^\]]*)\]\(([^)]+)\)",
                   r'<img src="\2" alt="\1" loading="lazy">', t)
        out.append(t)
    return "".join(out)

def render_table(lines):
    cells = lambda row: [c.strip() for c in row.strip().strip("|").split("|")]
    h = cells(lines[0])
    body = lines[2:]
    t = ("<div class=\"tablewrap\"><table><thead><tr>"
         + "".join(f"<th>{render_inline(x)}</th>" for x in h)
         + "</tr></thead><tbody>")
    for row in body:
        if not row.strip():
            continue
        t += "<tr>" + "".join(f"<td>{render_inline(x)}</td>" for x in cells(row)) + "</tr>"
    return t + "</tbody></table></div>\n"

def anchor_of(text: str) -> str:
    a = re.sub(r"[^a-z0-9\s-]", "", text.lower())
    return re.sub(r"\s+", "-", a).strip("-")

def render_block(s: str) -> str:
    lines = s.split("\n")
    out, i, n = [], 0, len(lines)
    para = []
    def flush_para():
        if para:
            out.append("<p>" + render_inline(" ".join(para)) + "</p>\n")
            para.clear()
    while i < n:
        line = lines[i]
        st = line.strip()
        if st.startswith("```"):
            flush_para()
            lang = st[3:].strip()
            i += 1
            buf = []
            while i < n and not lines[i].strip().startswith("```"):
                buf.append(lines[i]); i += 1
            i += 1
            out.append(f'<div class="codeblock"><pre><code class="language-{esc(lang)}">{esc(chr(10).join(buf))}</code></pre><button class="copy" title="Copiar">📋</button></div>\n')
            continue
        if st.startswith(":::"):
            flush_para()
            m = re.match(r"^:::\s*(\w+)\s*(.*)$", st)
            kind = m.group(1) if m else "info"
            label = m.group(2)
            i += 1
            buf = []
            while i < n and lines[i].strip() != ":::":
                buf.append(lines[i]); i += 1
            i += 1
            inner = " ".join(x.strip() for x in buf if x.strip())
            lbl = f"<b>{esc(label)}</b> " if label else ""
            out.append(f'<div class="callout {esc(kind)}">{lbl}{render_inline(inner)}</div>\n')
            continue
        m = re.match(r"^(#{1,4})\s+(.*)$", line)
        if m:
            flush_para()
            lvl = len(m.group(1))
            txt = render_inline(m.group(2))
            out.append(f'<h{lvl} id="{anchor_of(m.group(2))}">{txt}</h{lvl}>\n')
            i += 1
            continue
        if st.startswith("|") and i + 1 < n and re.match(r"^\s*\|[\s:|-]+\|?\s*$", lines[i + 1]):
            flush_para()
            j = i
            tbl = []
            while j < n and lines[j].strip().startswith("|"):
                tbl.append(lines[j]); j += 1
            out.append(render_table(tbl))
            i = j
            continue
        if re.match(r"^\s*[-*]\s+", line):
            flush_para()
            out.append("<ul>\n")
            while i < n and re.match(r"^\s*[-*]\s+", lines[i]):
                out.append("<li>" + render_inline(re.sub(r"^\s*[-*]\s+", "", lines[i])) + "</li>\n")
                i += 1
            out.append("</ul>\n")
            continue
        if re.match(r"^\s*\d+\.\s+", line):
            flush_para()
            out.append("<ol>\n")
            while i < n and re.match(r"^\s*\d+\.\s+", lines[i]):
                out.append("<li>" + render_inline(re.sub(r"^\s*\d+\.\s+", "", lines[i])) + "</li>\n")
                i += 1
            out.append("</ol>\n")
            continue
        if re.match(r"^\s*(---+|\*\*\*+)\s*$", line):
            flush_para(); out.append("<hr>\n"); i += 1; continue
        if st.startswith(">"):
            flush_para()
            buf = []
            while i < n and lines[i].strip().startswith(">"):
                buf.append(lines[i].strip()[1:].strip()); i += 1
            out.append("<blockquote>" + render_inline(" ".join(buf)) + "</blockquote>\n")
            continue
        if st == "":
            flush_para(); i += 1; continue
        para.append(line); i += 1
    flush_para()
    return "".join(out)

# ---------------------------------------------------------------------------
# Páginas
# ---------------------------------------------------------------------------

def parse_page(path):
    with open(path, encoding="utf-8") as f:
        raw = f.read()
    fm = {"title": os.path.splitext(os.path.basename(path))[0], "group": "", "order": 99}
    body = raw
    if raw.startswith("---"):
        end = raw.find("---", 3)
        if end != -1:
            meta = raw[3:end].strip()
            body = raw[end + 3:].strip()
            for line in meta.splitlines():
                if ":" in line:
                    k, v = line.split(":", 1)
                    fm[k.strip()] = v.strip()
    return fm, body

def collect_pages():
    pages = []
    for name in sorted(os.listdir(SRC)):
        if not name.endswith(".md"):
            continue
        path = os.path.join(SRC, name)
        fm, body = parse_page(path)
        fm["file"] = os.path.splitext(name)[0]
        pages.append((fm, body))
    def sort_key(p):
        fm = p[0]
        gi = GROUPS.index(fm["group"]) if fm["group"] in GROUPS else 99
        try:
            order = int(fm["order"])
        except ValueError:
            order = 99
        return (gi, order, fm["file"])
    pages.sort(key=sort_key)
    return pages

def sidebar(pages, active_file):
    by_group = {}
    for fm, _ in pages:
        by_group.setdefault(fm["group"], []).append(fm)
    nav = []
    for g in GROUPS:
        items = by_group.get(g, [])
        if not items:
            continue
        nav.append(f'<div class="group">{esc(g)}</div>')
        for fm in items:
            cls = "active" if fm["file"] == active_file else ""
            nav.append(f'<a href="{fm["file"]}.html" class="{cls}">{esc(fm["title"])}</a>')
    return "\n".join(nav)

def toc(body_html):
    heads = re.findall(r"<h([23]) id=\"([^\"]+)\">(.*?)</h[23]>", body_html)
    if len(heads) < 3:
        return ""
    items = "".join(f'<a href="#{h2}" class="t{int(h1)}">{re.sub("<[^>]+>", "", h3)}</a>' for h1, h2, h3 in heads)
    return f'<nav class="toc"><div class="toc-title">Nesta página</div>{items}</nav>'

def prev_next(pages, active_file):
    idx = next(i for i, (fm, _) in enumerate(pages) if fm["file"] == active_file)
    out = []
    if idx > 0:
        p = pages[idx - 1][0]
        out.append(f'<a class="navbtn prev" href="{p["file"]}.html"><small>← Anterior</small><span>{esc(p["title"])}</span></a>')
    else:
        out.append('<span class="navbtn"></span>')
    if idx < len(pages) - 1:
        nx = pages[idx + 1][0]
        out.append(f'<a class="navbtn next" href="{nx["file"]}.html"><small>Próximo →</small><span>{esc(nx["title"])}</span></a>')
    else:
        out.append('<span class="navbtn"></span>')
    return "\n".join(out)

def page_template(pages, fm, body, groups):
    title = esc(fm["title"])
    nav = sidebar(pages, fm["file"])
    content = render_block(body)
    pn = prev_next(pages, fm["file"])
    pg = toc(content)
    group = fm["group"]
    return f"""<!DOCTYPE html>
<html lang="pt-BR">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>{title} — {SITE_TITLE}</title>
<link rel="stylesheet" href="assets/style.css">
</head>
<body>
<div class="wrap">
<aside>
  <div class="brand">
    <div class="logo">K</div>
    <div><b>{SITE_TITLE}</b><small>{SITE_SUBTITLE} · {VERSION}</small></div>
  </div>
  <div class="search"><input id="filter" type="text" placeholder="Filtrar páginas…"></div>
  <nav id="toc">
{nav}
  </nav>
</aside>
<div class="overlay" id="overlay"></div>
<main>
<header class="top">
  <button class="burger" id="burger" aria-label="Menu">☰</button>
  <div class="crumbs"><span class="crumb">{esc(group)}</span><span class="sep">/</span><span>{title}</span></div>
  <div class="toggles">
    <a href="busca.html" class="btn">🔎 Buscar</a>
    <button class="btn" id="themeToggle" title="Alternar tema">☀️</button>
    <button class="btn" id="printBtn" title="Imprimir">🖨️</button>
    <a href="index.html" class="btn">Início</a>
  </div>
</header>
{pg}
<article>
{content}
</article>
<div class="pager">
{pn}
</div>
<footer class="footer">
  © Kizuri Studio · {VERSION} · <a href="index.html">Início</a> · <a href="busca.html">Buscar</a>
</footer>
</main>
</div>
<script src="assets/app.js"></script>
</body>
</html>
"""

def search_page(pages):
    items = []
    for fm, body in pages:
        text = re.sub(r"[#*`|>\-\[\]()]", " ", body)
        text = re.sub(r"\s+", " ", text)[:2500]
        items.append({"t": fm["title"], "g": fm["group"],
                      "u": fm["file"] + ".html", "s": text})
    idx = "window.SEARCH_INDEX = " + json.dumps(items, ensure_ascii=False)
    with open(os.path.join(OUT, "index.js"), "w", encoding="utf-8") as f:
        f.write(idx)
    return f"""<!DOCTYPE html>
<html lang="pt-BR">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Buscar — {SITE_TITLE}</title>
<link rel="stylesheet" href="assets/style.css">
</head>
<body>
<div class="wrap">
<aside>
  <div class="brand">
    <div class="logo">K</div>
    <div><b>{SITE_TITLE}</b><small>{SITE_SUBTITLE} · {VERSION}</small></div>
  </div>
  <div class="search"><input id="filter" type="text" placeholder="Filtrar páginas…"></div>
  <nav id="toc">
{sidebar(pages, "__none__")}
  </nav>
</aside>
<div class="overlay" id="overlay"></div>
<main>
<header class="top">
  <button class="burger" id="burger" aria-label="Menu">☰</button>
  <div class="crumbs">Busca</div>
  <div class="toggles">
    <button class="btn" id="themeToggle" title="Alternar tema">☀️</button>
    <a href="index.html" class="btn">Início</a>
  </div>
</header>
<article>
<h1>Buscar na documentação</h1>
<p><input id="q" type="search" class="bigsearch" placeholder="Ex.: raycast, sprite, física, painéis, SSR…"></p>
<div id="results"></div>
</article>
<footer class="footer">© Kizuri Studio · {VERSION}</footer>
</main>
</div>
<script src="index.js"></script>
<script src="assets/app.js"></script>
</body>
</html>
"""

# ---------------------------------------------------------------------------
# Assets
# ---------------------------------------------------------------------------

def asset_css():
    return r""":root{
  --bg:#0a0d13; --bg2:#0e1220; --bg3:#141a2b; --bg4:#1a2236;
  --border:#212a3f; --border2:#2b3654;
  --text:#dce4f2; --text-dim:#99a5bf; --text-faint:#667090;
  --accent:#5ad1ff; --accent2:#3be0ad; --accent3:#ff7b93; --gold:#f6cc82;
  --code-bg:#0c1119; --radius:10px; --sidebar-w:272px;
  --shadow:0 10px 30px rgba(0,0,0,.35);
}
html[data-theme="light"]{
  --bg:#f6f8fb; --bg2:#ffffff; --bg3:#eef1f7; --bg4:#e4e9f2;
  --border:#dfe4ee; --border2:#cdd5e4;
  --text:#1c2333; --text-dim:#4d5a75; --text-faint:#7b86a0;
  --accent:#0b7ab8; --accent2:#0f9d73; --accent3:#d43a5c; --gold:#a96f1c;
  --code-bg:#0c1119; --shadow:0 10px 30px rgba(20,30,60,.08);
}
*{box-sizing:border-box;margin:0;padding:0;}
html{scroll-behavior:smooth;scroll-padding-top:20px;}
body{background:var(--bg);color:var(--text);font-family:"Inter",-apple-system,"Segoe UI",Roboto,Arial,sans-serif;font-size:15.5px;line-height:1.72;}
a{color:var(--accent);text-decoration:none;}
a:hover{text-decoration:underline;}
.wrap{display:flex;min-height:100vh;}
aside{width:var(--sidebar-w);flex:0 0 var(--sidebar-w);position:sticky;top:0;height:100vh;overflow-y:auto;background:var(--bg2);border-right:1px solid var(--border);padding:18px 12px 40px;z-index:30;}
.brand{display:flex;align-items:center;gap:10px;padding:4px 8px 14px;border-bottom:1px solid var(--border);margin-bottom:12px;}
.brand .logo{width:36px;height:36px;border-radius:10px;background:linear-gradient(135deg,#5ad1ff,#2563eb);display:flex;align-items:center;justify-content:center;font-weight:800;font-size:17px;color:#fff;box-shadow:var(--shadow);}
.brand b{font-size:14.5px;letter-spacing:.4px;display:block;}
.brand small{color:var(--text-faint);font-size:11px;display:block;}
.search{margin:0 0 12px;padding:0 4px;}
.search input{width:100%;background:var(--bg3);border:1px solid var(--border);border-radius:8px;color:var(--text);padding:8px 10px;font-size:13px;outline:none;}
.search input:focus{border-color:var(--accent);}
nav#toc a{display:block;color:var(--text-dim);padding:5.5px 10px;border-radius:7px;font-size:13.5px;margin:1px 0;border-left:2px solid transparent;transition:background .12s,color .12s;}
nav#toc a:hover{background:var(--bg3);color:var(--text);text-decoration:none;}
nav#toc a.active{background:var(--bg4);color:var(--accent);border-left-color:var(--accent);font-weight:600;}
nav#toc .group{font-size:10.5px;text-transform:uppercase;letter-spacing:1.2px;color:var(--text-faint);margin:16px 8px 4px;font-weight:700;}
main{flex:1;max-width:900px;margin:0 auto;padding:28px 48px 110px;min-width:0;}
header.top{display:flex;align-items:center;justify-content:space-between;gap:10px;border-bottom:1px solid var(--border);padding-bottom:14px;margin-bottom:22px;flex-wrap:wrap;}
.burger{display:none;background:none;border:1px solid var(--border);color:var(--text);border-radius:8px;padding:4px 10px;font-size:16px;cursor:pointer;}
.crumbs{color:var(--text-faint);font-size:13px;}
.crumbs .sep{margin:0 6px;}
.toggles{display:flex;gap:8px;align-items:center;flex-wrap:wrap;}
.btn{color:var(--text-dim);border:1px solid var(--border2);border-radius:8px;padding:5px 12px;font-size:13px;background:var(--bg3);cursor:pointer;font-family:inherit;}
.btn:hover{border-color:var(--accent);color:var(--accent);text-decoration:none;}
nav.toc{background:var(--bg2);border:1px solid var(--border);border-left:3px solid var(--accent);border-radius:var(--radius);padding:12px 16px;margin:0 0 22px;font-size:13px;}
nav.toc .toc-title{font-weight:700;font-size:11px;text-transform:uppercase;letter-spacing:1px;color:var(--text-faint);margin-bottom:6px;}
nav.toc a{display:block;color:var(--text-dim);padding:2px 0;}
nav.toc a.t3{padding-left:14px;color:var(--text-faint);}
article h1{font-size:28px;margin:0 0 18px;line-height:1.25;}
article h2{font-size:21px;margin:44px 0 12px;padding-bottom:7px;border-bottom:1px solid var(--border);}
article h3{font-size:17px;margin:30px 0 10px;}
article h4{font-size:15px;margin:22px 0 8px;color:var(--text-dim);}
article p{margin:11px 0;}
article ul,article ol{margin:11px 0 11px 24px;}
article li{margin:5px 0;}
article img{max-width:100%;border-radius:var(--radius);border:1px solid var(--border);}
article code{background:var(--code-bg);border:1px solid var(--border);border-radius:5px;padding:1.5px 6px;font-family:"JetBrains Mono",ui-monospace,Consolas,monospace;font-size:12.5px;color:#9fd8f7;}
html[data-theme="light"] article code{color:#0b5e8a;}
.codeblock{position:relative;margin:14px 0;}
article pre{background:var(--code-bg);border:1px solid var(--border);border-radius:var(--radius);padding:16px 18px;overflow-x:auto;line-height:1.55;}
article pre code{background:none;border:none;padding:0;font-size:13px;color:var(--text);}
.copy{position:absolute;top:8px;right:8px;background:var(--bg3);border:1px solid var(--border2);color:var(--text-dim);border-radius:6px;padding:3px 8px;font-size:12px;cursor:pointer;opacity:.7;font-family:inherit;}
.copy:hover{opacity:1;color:var(--accent);}
.copy.done{color:var(--accent2);border-color:var(--accent2);}
.tok-kw{color:#c792ea;font-weight:600;}
.tok-str{color:#a5e075;}
.tok-num{color:#f78c6c;}
.tok-com{color:#5c6779;font-style:italic;}
.tok-cls{color:#ffcb6b;}
.tok-fn{color:#82aaff;}
html[data-theme="light"] .tok-kw{color:#7a3cc0;} html[data-theme="light"] .tok-str{color:#3c7a1e;}
html[data-theme="light"] .tok-num{color:#b3561a;} html[data-theme="light"] .tok-cls{color:#9a6a00;}
html[data-theme="light"] .tok-com{color:#8a93a6;} html[data-theme="light"] .tok-fn{color:#1d5bb8;}
.tablewrap{overflow-x:auto;margin:14px 0;}
article table{border-collapse:collapse;width:100%;font-size:13.5px;}
article th,article td{border:1px solid var(--border);padding:8px 11px;text-align:left;vertical-align:top;}
article th{background:var(--bg3);font-weight:600;}
article tr:nth-child(even) td{background:rgba(128,128,160,.04);}
article blockquote{border-left:3px solid var(--accent);background:var(--bg2);padding:10px 16px;border-radius:0 8px 8px 0;margin:14px 0;color:var(--text-dim);}
.callout{border-radius:var(--radius);padding:13px 16px;margin:16px 0;border:1px solid;font-size:14px;}
.callout.info{background:rgba(90,209,255,.08);border-color:rgba(90,209,255,.35);}
.callout.info b{color:var(--accent);}
.callout.dica,.callout.tip{background:rgba(59,224,173,.08);border-color:rgba(59,224,173,.4);}
.callout.dica b,.callout.tip b{color:var(--accent2);}
.callout.nota,.callout.note{background:rgba(246,204,130,.08);border-color:rgba(246,204,130,.4);}
.callout.nota b,.callout.note b{color:var(--gold);}
.callout.aviso,.callout.warn{background:rgba(255,123,147,.08);border-color:rgba(255,123,147,.4);}
.callout.aviso b,.callout.warn b{color:var(--accent3);}
.pager{display:flex;justify-content:space-between;gap:12px;margin-top:52px;padding-top:18px;border-top:1px solid var(--border);}
.navbtn{flex:1;text-decoration:none;color:var(--text-dim);background:var(--bg2);border:1px solid var(--border);border-radius:10px;padding:12px 16px;display:flex;flex-direction:column;gap:3px;}
.navbtn small{font-size:11.5px;color:var(--text-faint);}
.navbtn span{font-size:14px;font-weight:600;}
a.navbtn:hover{border-color:var(--accent);color:var(--accent);text-decoration:none;}
.navbtn.next{text-align:right;align-items:flex-end;}
.footer{margin-top:44px;padding-top:14px;border-top:1px solid var(--border);color:var(--text-faint);font-size:12.5px;}
.footer a{color:var(--text-dim);}
.bigsearch{width:100%;background:var(--bg3);border:1px solid var(--border2);border-radius:10px;color:var(--text);padding:13px 16px;font-size:16px;outline:none;}
.bigsearch:focus{border-color:var(--accent);}
#results .hit{background:var(--bg2);border:1px solid var(--border);border-radius:8px;padding:12px 16px;margin:10px 0;}
#results .hit a{font-weight:600;font-size:15px;}
#results .hit p{color:var(--text-dim);font-size:13.5px;margin:4px 0 0;}
mark{background:rgba(90,209,255,.22);color:var(--accent);border-radius:3px;padding:0 2px;}
@media (max-width:980px){
  aside{transform:translateX(-100%);position:fixed;transition:transform .2s;box-shadow:var(--shadow);}
  body.menu-open aside{transform:translateX(0);}
  .burger{display:inline-block;}
  .overlay{display:none;position:fixed;inset:0;background:rgba(0,0,0,.45);z-index:20;}
  body.menu-open .overlay{display:block;}
  main{padding:20px 18px 80px;}
}
@media print{.wrap{display:block;}aside,.toggles,.pager,.toc,.copy{display:none!important;}main{padding:0;max-width:none;}}
"""

def asset_js():
    return r"""
// sidebar: filtrar páginas
(function(){
  var input = document.getElementById('filter');
  if (!input) return;
  var links = Array.prototype.slice.call(document.querySelectorAll('#toc a'));
  input.addEventListener('input', function(){
    var q = input.value.toLowerCase();
    links.forEach(function(a){
      a.style.display = (q === '' || a.textContent.toLowerCase().indexOf(q) !== -1) ? '' : 'none';
    });
  });
})();

// sidebar: destaque por scroll
(function(){
  var links = Array.prototype.slice.call(document.querySelectorAll('#toc a'));
  var heads = Array.prototype.slice.call(document.querySelectorAll('article h2,article h3'));
  if (!heads.length) return;
  function setActive(id){
    links.forEach(function(a){
      a.classList.toggle('active', a.getAttribute('href') === '#' + id || a.getAttribute('href') === location.pathname.split('/').pop());
    });
  }
  function onScroll(){
    var pos = window.scrollY + 90, cur = heads[0].id;
    heads.forEach(function(h){ if (h.offsetTop <= pos) cur = h.id; });
    setActive(cur);
  }
  window.addEventListener('scroll', onScroll, {passive:true});
  onScroll();
})();

// tema claro/escuro
(function(){
  var btn = document.getElementById('themeToggle');
  if (!btn) return;
  var saved = null;
  try { saved = localStorage.getItem('kizuri-theme'); } catch(e) {}
  if (saved === 'light') document.documentElement.dataset.theme = 'light';
  function update(){ btn.textContent = document.documentElement.dataset.theme === 'light' ? '🌙' : '☀️'; }
  update();
  btn.addEventListener('click', function(){
    var light = document.documentElement.dataset.theme === 'light';
    if (light) delete document.documentElement.dataset.theme;
    else document.documentElement.dataset.theme = 'light';
    try { localStorage.setItem('kizuri-theme', light ? 'dark' : 'light'); } catch(e) {}
    update();
  });
})();

// menu mobile
(function(){
  var burger = document.getElementById('burger');
  var overlay = document.getElementById('overlay');
  if (!burger) return;
  burger.addEventListener('click', function(){ document.body.classList.toggle('menu-open'); });
  if (overlay) overlay.addEventListener('click', function(){ document.body.classList.remove('menu-open'); });
})();

// impressão
(function(){
  var b = document.getElementById('printBtn');
  if (b) b.addEventListener('click', function(){ window.print(); });
})();

// copiar código
(function(){
  document.querySelectorAll('.codeblock').forEach(function(blk){
    var btn = blk.querySelector('.copy');
    var code = blk.querySelector('pre code');
    if (!btn || !code) return;
    btn.addEventListener('click', function(){
      navigator.clipboard.writeText(code.textContent).then(function(){
        btn.textContent = '✓';
        btn.classList.add('done');
        setTimeout(function(){ btn.textContent = '📋'; btn.classList.remove('done'); }, 1200);
      });
    });
  });
})();

// destaque de sintaxe
(function(){
  var RULES = {
    csharp: [
      [/\/\/.*|\/\*[\s\S]*?\*\//g, 'tok-com'],
      [/"(?:[^"\\]|\\.)*"/g, 'tok-str'],
      [/\b(?:public|private|protected|internal|static|sealed|abstract|class|struct|interface|enum|namespace|using|new|return|if|else|for|foreach|while|do|var|out|ref|in|override|virtual|readonly|const|this|base|void|int|float|double|bool|string|uint|yield|true|false|null|get|set)\b/g, 'tok-kw'],
      [/\b[A-Z][A-Za-z0-9_]*\b/g, 'tok-cls'],
      [/\b[0-9]+(?:\.[0-9]+)?f?\b/g, 'tok-num']
    ],
    cpp: [
      [/\/\/.*|\/\*[\s\S]*?\*\//g, 'tok-com'],
      [/"(?:[^"\\]|\\.)*"/g, 'tok-str'],
      [/\b(?:struct|class|namespace|using|public|private|static|void|int|float|bool|const|auto|return|new|if|else|for|while)\b/g, 'tok-kw'],
      [/\b[A-Z][A-Za-z0-9_]*\b/g, 'tok-cls'],
      [/\b[0-9]+(?:\.[0-9]+)?f?\b/g, 'tok-num']
    ],
    bash: [
      [/[#][^\n]*/g, 'tok-com'],
      [/"(?:[^"\\]|\\.)*"/g, 'tok-str'],
      [/\b(?:cd|cmake|build|run|preset|ls|mkdir|cp|mv|export|pip|python3)\b/g, 'tok-kw']
    ],
    glsl: [
      [/\/\/.*/g, 'tok-com'],
      [/\b(?:in|out|uniform|layout|vec2|vec3|vec4|mat4|float|int|bool|if|else|for|return|texture|discard)\b/g, 'tok-kw']
    ],
    json: [
      [/"(?:[^"\\]|\\.)*"(?=\s*:)/g, 'tok-kw'],
      [/"(?:[^"\\]|\\.)*"/g, 'tok-str'],
      [/\b-?[0-9.]+f?\b/g, 'tok-num']
    ],
    text: []
  };
  function escHtml(s){ return s.replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;'); }
  document.querySelectorAll('pre code').forEach(function(el){
    var lang = (el.className.match(/language-(\w+)/) || [])[1] || '';
    var rules = RULES[lang] || [];
    if (!rules.length) return;
    var re = new RegExp(rules.map(function(r){ return r[0].source; }).join('|'), 'g');
    var map = rules.map(function(r){ return r[1]; });
    var out = '', last = 0, m;
    while ((m = re.exec(el.textContent)) !== null){
      var cls = '';
      for (var i = 0; i < rules.length; ++i) if (m[i + 1] !== undefined){ cls = map[i]; break; }
      out += escHtml(el.textContent.slice(last, m.index));
      out += '<span class="' + cls + '">' + escHtml(m[0]) + '</span>';
      last = re.lastIndex;
    }
    out += escHtml(el.textContent.slice(last));
    el.innerHTML = out;
  });
})();

// busca
(function(){
  var q = document.getElementById('q');
  var res = document.getElementById('results');
  if (!q || !res || typeof window.SEARCH_INDEX === 'undefined') return;
  function escHtml(s){ return s.replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;'); }
  function render(){
    var term = q.value.trim().toLowerCase();
    res.innerHTML = '';
    if (!term) return;
    var hits = 0;
    window.SEARCH_INDEX.forEach(function(page){
      if (!page.s.toLowerCase().includes(term)) return;
      hits++;
      var i = page.s.toLowerCase().indexOf(term);
      var start = Math.max(0, i - 60), len = Math.min(page.s.length - start, 220);
      var ctx = page.s.substr(start, len);
      if (start > 0) ctx = '…' + ctx;
      var d = document.createElement('div');
      d.className = 'hit';
      d.innerHTML = '<a href="' + page.u + '">' + escHtml(page.t) + '</a>' +
        '<span style="color:var(--text-faint);font-size:12px;margin-left:8px">' + escHtml(page.g) + '</span>' +
        '<p>' + ctx.replace(new RegExp('(' + term.replace(/[.*+?^${}()|[\]\\]/g, '\\$&') + ')', 'gi'), '<mark>$1</mark>') + '</p>';
      res.appendChild(d);
    });
    if (!hits) res.innerHTML = '<p style="color:var(--text-faint)">Nada encontrado para “' + escHtml(term) + '”.</p>';
  }
  q.addEventListener('input', render);
  var params = new URLSearchParams(location.search);
  if (params.get('q')){ q.value = params.get('q'); render(); }
})();
"""

def write_site():
    os.makedirs(OUT, exist_ok=True)
    os.makedirs(os.path.join(OUT, "assets"), exist_ok=True)
    pages = collect_pages()
    if not pages:
        sys.exit("Nenhuma página .md encontrada em src/")
    for fm, body in pages:
        with open(os.path.join(OUT, fm["file"] + ".html"), "w", encoding="utf-8") as f:
            f.write(page_template(pages, fm, body, GROUPS))
    with open(os.path.join(OUT, "busca.html"), "w", encoding="utf-8") as f:
        f.write(search_page(pages))
    with open(os.path.join(OUT, "assets", "style.css"), "w", encoding="utf-8") as f:
        f.write(asset_css())
    with open(os.path.join(OUT, "assets", "app.js"), "w", encoding="utf-8") as f:
        f.write(asset_js())
    print(f"Site gerado em {OUT}: {len(pages)} páginas + busca.")

if __name__ == "__main__":
    write_site()
