#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Gerador do site de documentação da Kizuri Engine.

Lê os arquivos Markdown em src/ (um por página, com front matter) e gera um
site estático multi-página em site/: páginas HTML interligadas com sidebar
agrupada, breadcrumb, prev/next, busca e destaque de sintaxe. Só usa a
biblioteca padrão (sem dependências). O site é standalone — abre de file://
e pode ser hospedado em qualquer lugar (GitHub Pages, nginx, etc.), NÃO vai
dentro do pacote da engine.

Front matter de cada página:
    ---
    title:   Título da página
    group:   Grupo da sidebar (ex.: "Componentes")
    order:   1
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
GROUPS = ["Introdução", "Conceitos", "Editor", "Componentes", "Renderização",
          "Scripting C#", "Distribuição"]

# ---------------------------------------------------------------------------
# Markdown -> HTML
# ---------------------------------------------------------------------------

def esc(s: str) -> str:
    return html.escape(s, quote=False)

# Quebra por linha de fenced code + tabelas para não tocar em nada dentro deles
def render_inline(s: str) -> str:
    # protege spans de código
    parts = []
    pos = 0
    for m in re.finditer(r"`([^`]+)`", s):
        parts.append((s[pos:m.start()], True))
        parts.append((m.group(1), False))
        pos = m.end()
    parts.append((s[pos:], True))
    out = []
    for text, is_code in parts:
        if not is_code:
            out.append('<code>' + esc(text) + '</code>')
            continue
        t = text
        t = re.sub(r"\[([^\]]+)\]\(([^)]+)\)",
                   r'<a href="\2">\1</a>', t)
        t = re.sub(r"\*\*([^*]+)\*\*", r"<strong>\1</strong>", t)
        t = re.sub(r"(?<!\*)\*([^*]+)\*(?!\*)", r"<em>\1</em>", t)
        out.append(t)
    return "".join(out)

def render_table(lines):
    # lines: cabeçalho, separador |---|, linhas de dados
    head = lines[0]
    cells = lambda row: [c.strip() for c in row.strip().strip("|").split("|")]
    h = cells(head)
    body = lines[2:]
    t = "<table><thead><tr>" + "".join(f"<th>{render_inline(x)}</th>" for x in h) + "</tr></thead><tbody>"
    for row in body:
        if not row.strip():
            continue
        t += "<tr>" + "".join(f"<td>{render_inline(x)}</td>" for x in cells(row)) + "</tr>"
    return t + "</tbody></table>"

def render_block(s: str) -> str:
    lines = s.split("\n")
    out = []
    i = 0
    n = len(lines)
    def flush_para(buf):
        if buf:
            out.append("<p>" + render_inline(" ".join(buf)) + "</p>\n")
            buf.clear()  # SEM isso o parágrafo era re-escrito em todo flush seguinte (duplicação!)
    para = []
    while i < n:
        line = lines[i]
        stripped = line.strip()
        # fenced code
        if stripped.startswith("```"):
            flush_para(para)
            lang = stripped[3:].strip()
            i += 1
            buf = []
            while i < n and not lines[i].strip().startswith("```"):
                buf.append(lines[i])
                i += 1
            i += 1  # fecha
            out.append(f'<pre><code class="language-{esc(lang)}">{esc(chr(10).join(buf))}</code></pre>\n')
            continue
        # callout (:::[tipo] ... :::)
        if stripped.startswith(":::"):
            flush_para(para)
            m = re.match(r"^:::\s*(\w+)\s*(.*)$", stripped)
            kind = m.group(1) if m else "info"
            label = m.group(2)
            i += 1
            buf = []
            while i < n and lines[i].strip() != ":::":
                buf.append(lines[i])
                i += 1
            i += 1
            inner = " ".join(x.strip() for x in buf if x.strip())
            body = render_inline(inner)
            lbl = f"<b>{esc(label)}</b> " if label else ""
            out.append(f'<div class="callout {esc(kind)}">{lbl}{body}</div>\n')
            continue
        # heading
        m = re.match(r"^(#{1,4})\s+(.*)$", line)
        if m:
            flush_para(para)
            lvl = len(m.group(1))
            txt = render_inline(m.group(2))
            anchor = re.sub(r"[^a-z0-9áéíóúãõâêôç\s-]", "", m.group(2).lower())
            anchor = re.sub(r"\s+", "-", anchor).strip("-")
            out.append(f'<h{lvl} id="{anchor}">{txt}</h{lvl}>\n')
            i += 1
            continue
        # table
        if stripped.startswith("|") and i + 1 < n and re.match(r"^\s*\|[\s:|-]+\|?\s*$", lines[i + 1]):
            flush_para(para)
            j = i
            tbl = []
            while j < n and lines[j].strip().startswith("|"):
                tbl.append(lines[j])
                j += 1
            out.append(render_table(tbl) + "\n")
            i = j
            continue
        # list
        if re.match(r"^\s*[-*]\s+", line):
            flush_para(para)
            out.append("<ul>\n")
            while i < n and re.match(r"^\s*[-*]\s+", lines[i]):
                out.append("<li>" + render_inline(re.sub(r"^\s*[-*]\s+", "", lines[i])) + "</li>\n")
                i += 1
            out.append("</ul>\n")
            continue
        if re.match(r"^\s*\d+\.\s+", line):
            flush_para(para)
            out.append("<ol>\n")
            while i < n and re.match(r"^\s*\d+\.\s+", lines[i]):
                out.append("<li>" + render_inline(re.sub(r"^\s*\d+\.\s+", "", lines[i])) + "</li>\n")
                i += 1
            out.append("</ol>\n")
            continue
        # hr
        if re.match(r"^\s*(---+|\*\*\*+)\s*$", line):
            flush_para(para)
            out.append("<hr>\n")
            i += 1
            continue
        # blockquote simples
        if stripped.startswith(">"):
            flush_para(para)
            buf = []
            while i < n and lines[i].strip().startswith(">"):
                buf.append(lines[i].strip()[1:].strip())
                i += 1
            out.append("<blockquote>" + render_inline(" ".join(buf)) + "</blockquote>\n")
            continue
        if stripped == "":
            flush_para(para)
            i += 1
            continue
        para.append(line)
        i += 1
    flush_para(para)
    return "".join(out)

# ---------------------------------------------------------------------------
# Leitura das páginas
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

# ---------------------------------------------------------------------------
# Template
# ---------------------------------------------------------------------------

def sidebar(pages, active_file, groups):
    nav = []
    by_group = {}
    for fm, _ in pages:
        by_group.setdefault(fm["group"], []).append(fm)
    for g in groups:
        items = by_group.get(g, [])
        if not items:
            continue
        nav.append(f'<div class="group">{esc(g)}</div>')
        for fm in items:
            cls = "active" if fm["file"] == active_file else ""
            nav.append(f'<a href="{fm["file"]}.html" class="{cls}">{esc(fm["title"])}</a>')
    return "\n".join(nav)

def breadcrumb(fm, groups):
    g = fm["group"]
    return f'<span class="crumb">{esc(g)}</span><span class="sep">/</span><span>{esc(fm["title"])}</span>'

def prev_next(pages, active_file):
    idx = next(i for i, (fm, _) in enumerate(pages) if fm["file"] == active_file)
    out = []
    if idx > 0:
        p = pages[idx - 1][0]
        out.append(f'<a class="navbtn prev" href="{p["file"]}.html">← {esc(p["title"])}</a>')
    else:
        out.append('<span class="navbtn"></span>')
    if idx < len(pages) - 1:
        nx = pages[idx + 1][0]
        out.append(f'<a class="navbtn next" href="{nx["file"]}.html">{esc(nx["title"])} →</a>')
    else:
        out.append('<span class="navbtn"></span>')
    return "\n".join(out)

def page_template(pages, fm, body, groups):
    title = esc(fm["title"])
    nav = sidebar(pages, fm["file"], groups)
    bc = breadcrumb(fm, groups)
    pn = prev_next(pages, fm["file"])
    active = fm["file"]
    content = render_block(body)
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
    <div><b>{SITE_TITLE}</b><small>{SITE_SUBTITLE}</small></div>
  </div>
  <div class="search"><input id="filter" type="text" placeholder="Filtrar páginas…"></div>
  <nav id="toc">
{nav}
  </nav>
</aside>
<main>
<header class="top">
  <div class="crumbs">{bc}</div>
  <div class="toggles">
    <a href="busca.html" class="btn">🔎 Buscar</a>
    <a href="index.html" class="btn">Início</a>
  </div>
</header>
<article>
{content}
</article>
<div class="pager">
{pn}
</div>
<footer class="footer">
  Kizuri Engine — documentação (site estático). Fonte em <code>documentacao/src/</code>;
  gerado por <code>documentacao/generate_site.py</code>.
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
        text = re.sub(r"\s+", " ", text)[:2200]
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
    <div><b>{SITE_TITLE}</b><small>{SITE_SUBTITLE}</small></div>
  </div>
  <div class="search"><input id="filter" type="text" placeholder="Filtrar páginas…"></div>
  <nav id="toc">
{sidebar(pages, "__none__", GROUPS)}
  </nav>
</aside>
<main>
<header class="top">
  <div class="crumbs">Busca</div>
  <div class="toggles"><a href="index.html" class="btn">Início</a></div>
</header>
<article>
<h1>Buscar na documentação</h1>
<p><input id="q" type="search" class="bigsearch" placeholder="Ex.: raycast, sprite, física, PlayOneShotAt…"></p>
<div id="results"></div>
</article>
<footer class="footer">
  Kizuri Engine — busca (client-side, sem servidor).
</footer>
</main>
</div>
<script src="index.js"></script>
<script src="assets/app.js"></script>
</body>
</html>
"""

def asset_css():
    return """:root{
  --bg:#0b0e14; --bg2:#0f131c; --bg3:#151a26; --bg4:#1a2130;
  --border:#232b3d; --border2:#2c3650;
  --text:#dbe2ee; --text-dim:#9aa6bd; --text-faint:#6b7690;
  --accent:#4fc3f7; --accent2:#38d9a9; --accent3:#ff6b81; --gold:#f5c97b;
  --code-bg:#0d1117; --radius:10px; --sidebar-w:270px;
}
*{box-sizing:border-box;margin:0;padding:0;}
html{scroll-behavior:smooth;scroll-padding-top:24px;}
body{background:var(--bg);color:var(--text);font-family:"Inter",-apple-system,"Segoe UI",Roboto,Arial,sans-serif;font-size:15.5px;line-height:1.7;}
.wrap{display:flex;min-height:100vh;}
aside{width:var(--sidebar-w);flex:0 0 var(--sidebar-w);position:sticky;top:0;height:100vh;overflow-y:auto;background:var(--bg2);border-right:1px solid var(--border);padding:18px 12px 40px;}
.brand{display:flex;align-items:center;gap:10px;padding:4px 8px 14px;border-bottom:1px solid var(--border);margin-bottom:12px;}
.brand .logo{width:34px;height:34px;border-radius:9px;background:linear-gradient(135deg,#4fc3f7,#2563eb);display:flex;align-items:center;justify-content:center;font-weight:800;font-size:16px;color:#fff;}
.brand b{font-size:14.5px;letter-spacing:.4px;display:block;}
.brand small{color:var(--text-faint);font-size:11px;display:block;}
.search{margin:0 0 12px;padding:0 4px;}
.search input{width:100%;background:var(--bg3);border:1px solid var(--border);border-radius:8px;color:var(--text);padding:8px 10px;font-size:13px;outline:none;}
.search input:focus{border-color:var(--accent);}
nav a{display:block;color:var(--text-dim);text-decoration:none;padding:5.5px 10px;border-radius:7px;font-size:13.5px;margin:1px 0;border-left:2px solid transparent;}
nav a:hover{background:var(--bg3);color:var(--text);}
nav a.active{background:var(--bg4);color:var(--accent);border-left-color:var(--accent);}
nav .group{font-size:11px;text-transform:uppercase;letter-spacing:1px;color:var(--text-faint);margin:16px 8px 4px;}
main{flex:1;max-width:860px;margin:0 auto;padding:30px 44px 110px;}
header.top{display:flex;align-items:center;justify-content:space-between;gap:12px;border-bottom:1px solid var(--border);padding-bottom:14px;margin-bottom:26px;flex-wrap:wrap;}
.crumbs{color:var(--text-faint);font-size:13px;}
.crumbs .sep{margin:0 6px;}
.toggles{display:flex;gap:8px;}
.btn{color:var(--accent);border:1px solid var(--border2);border-radius:8px;padding:5px 12px;font-size:13px;text-decoration:none;background:var(--bg3);}
.btn:hover{border-color:var(--accent);}
article h1{font-size:27px;margin:0 0 18px;}
article h2{font-size:21px;margin:44px 0 12px;padding-bottom:7px;border-bottom:1px solid var(--border);}
article h3{font-size:17px;margin:30px 0 10px;color:#eef2f9;}
article h4{font-size:15px;margin:22px 0 8px;color:#d7e0ee;}
article p{margin:11px 0;}
article ul,article ol{margin:11px 0 11px 24px;}
article li{margin:5px 0;}
article a{color:var(--accent);}
article strong{color:#eef2f9;}
article code{background:var(--code-bg);border:1px solid var(--border);border-radius:5px;padding:1.5px 6px;font-family:"JetBrains Mono",ui-monospace,Consolas,monospace;font-size:12.5px;color:#9fd3f5;}
article pre{background:var(--code-bg);border:1px solid var(--border);border-radius:var(--radius);padding:16px 18px;overflow-x:auto;margin:14px 0;line-height:1.55;}
article pre code{background:none;border:none;padding:0;font-size:13px;color:var(--text);}
.tok-kw{color:#c792ea;font-weight:600;}
.tok-str{color:#a5e075;}
.tok-num{color:#f78c6c;}
.tok-com{color:#5c6779;font-style:italic;}
.tok-cls{color:#ffcb6b;}
.tok-fn{color:#82aaff;}
article table{border-collapse:collapse;width:100%;margin:14px 0;font-size:13.5px;}
article th,article td{border:1px solid var(--border);padding:8px 11px;text-align:left;vertical-align:top;}
article th{background:var(--bg3);color:#c9d6ea;font-weight:600;}
article tr:nth-child(even) td{background:rgba(255,255,255,.015);}
article blockquote{border-left:3px solid var(--accent);background:var(--bg2);padding:10px 16px;border-radius:0 8px 8px 0;margin:14px 0;color:var(--text-dim);}
.callout{border-radius:var(--radius);padding:13px 16px;margin:16px 0;border:1px solid;font-size:14px;}
.callout.info{background:rgba(79,195,247,.08);border-color:rgba(79,195,247,.35);}
.callout.info b{color:var(--accent);}
.callout.warn{background:rgba(245,201,123,.08);border-color:rgba(245,201,123,.4);}
.callout.warn b{color:var(--gold);}
.callout.ok{background:rgba(56,217,169,.08);border-color:rgba(56,217,169,.4);}
.callout.ok b{color:var(--accent2);}
.callout.danger{background:rgba(255,107,129,.08);border-color:rgba(255,107,129,.4);}
.callout.danger b{color:var(--accent3);}
.pager{display:flex;justify-content:space-between;gap:12px;margin-top:48px;padding-top:16px;border-top:1px solid var(--border);}
.navbtn{flex:1;text-decoration:none;color:var(--text-dim);background:var(--bg2);border:1px solid var(--border);border-radius:8px;padding:10px 14px;font-size:14px;}
a.navbtn:hover{border-color:var(--accent);color:var(--accent);}
.navbtn.next{text-align:right;}
.footer{margin-top:44px;padding-top:14px;border-top:1px solid var(--border);color:var(--text-faint);font-size:12.5px;}
.bigsearch{width:100%;background:var(--bg3);border:1px solid var(--border2);border-radius:10px;color:var(--text);padding:13px 16px;font-size:16px;outline:none;}
.bigsearch:focus{border-color:var(--accent);}
#results .hit{background:var(--bg2);border:1px solid var(--border);border-radius:8px;padding:12px 16px;margin:10px 0;}
#results .hit a{font-weight:600;font-size:15px;}
#results .hit p{color:var(--text-dim);font-size:13.5px;margin:4px 0 0;}
mark{background:rgba(79,195,247,.25);color:var(--accent);border-radius:3px;padding:0 2px;}
@media (max-width:860px){aside{display:none;}main{padding:22px 18px 80px;}}
"""

def asset_js():
    return """// filtro de páginas da sidebar
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

// destaque da página ativa (por scroll) — apenas âncoras, sem intersecção
(function(){
  var links = Array.prototype.slice.call(document.querySelectorAll('#toc a'));
  function active(h){
    links.forEach(function(a){
      a.classList.toggle('active', a.getAttribute('href') === h || a.getAttribute('href') === location.pathname.split('/').pop());
    });
  }
  active(null);
  var heads = Array.prototype.slice.call(document.querySelectorAll('article h2,article h3'));
  if (!heads.length) return;
  function onScroll(){
    var pos = window.scrollY + 100, cur = heads[0].id;
    heads.forEach(function(h){ if (h.offsetTop <= pos) cur = h.id; });
    links.forEach(function(a){
      a.classList.toggle('active', a.getAttribute('href') === '#' + cur || a.getAttribute('href') === location.pathname.split('/').pop());
    });
  }
  window.addEventListener('scroll', onScroll, {passive:true});
  onScroll();
})();

// syntax highlight (escapa HTML e pinta keywords/strings/números/comentários)
(function(){
  var RULES = {
    csharp: [
      [/\\/\\/.*|\\/\\*[\\s\\S]*?\\*\\//g, 'tok-com'],
      [/"(?:[^"\\\\]|\\\\.)*"/g, 'tok-str'],
      [/\\b(?:public|private|protected|internal|static|sealed|abstract|class|struct|interface|enum|namespace|using|new|return|if|else|for|foreach|while|do|var|out|ref|in|override|virtual|readonly|const|this|base|void|int|float|double|bool|string|uint|yield|true|false|null|get|set)\\b/g, 'tok-kw'],
      [/\\b[A-Z][A-Za-z0-9_]*\\b/g, 'tok-cls'],
      [/\\b[0-9]+(?:\\.[0-9]+)?f?\\b/g, 'tok-num']
    ],
    cpp: [
      [/\\/\\/.*|\\/\\*[\\s\\S]*?\\*\\//g, 'tok-com'],
      [/"(?:[^"\\\\]|\\\\.)*"/g, 'tok-str'],
      [/\\b(?:struct|class|namespace|using|public|private|static|void|int|float|bool|const|auto|return|new|if|else|for|while)\\b/g, 'tok-kw'],
      [/\\b[A-Z][A-Za-z0-9_]*\\b/g, 'tok-cls'],
      [/\\b[0-9]+(?:\\.[0-9]+)?f?\\b/g, 'tok-num']
    ],
    bash: [
      [/[#][^\\n]*/g, 'tok-com'],
      [/"(?:[^"\\\\]|\\\\.)*"/g, 'tok-str'],
      [/\\b(?:cd|cmake|build|run|preset|ls|mkdir|cp|mv|export)\\b/g, 'tok-kw']
    ],
    glsl: [
      [/\\/\\/.*/g, 'tok-com'],
      [/\\b(?:in|out|uniform|layout|vec2|vec3|vec4|mat4|float|int|bool|if|else|for|return|texture|discard)\\b/g, 'tok-kw']
    ],
    json: [
      [/"(?:[^"\\\\]|\\\\.)*"(?=\\s*:)/g, 'tok-kw'],
      [/"(?:[^"\\\\]|\\\\.)*"/g, 'tok-str'],
      [/\\b-?[0-9.]+\\b/g, 'tok-num']
    ],
    text: []
  };
  function esc(s){ return s.replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;'); }
  function highlight(code, lang){
    var rules = RULES[lang] || [];
    if (!rules.length) return esc(code);
    var re = new RegExp(rules.map(function(r){ return r[0].source; }).join('|'), 'g');
    var map = rules.map(function(r){ return r[1]; });
    var out = '', last = 0, m;
    while ((m = re.exec(code)) !== null){
      var cls = '';
      for (var i = 0; i < rules.length; ++i) if (m[i + 1] !== undefined){ cls = map[i]; break; }
      out += esc(code.slice(last, m.index));
      out += '<span class="' + cls + '">' + esc(m[0]) + '</span>';
      last = re.lastIndex;
    }
    out += esc(code.slice(last));
    return out;
  }
  document.querySelectorAll('pre code').forEach(function(el){
    var lang = (el.className.match(/language-(\\w+)/) || [])[1] || '';
    el.innerHTML = highlight(el.textContent, lang);
  });
})();

// página de busca (client-side)
(function(){
  var q = document.getElementById('q');
  var res = document.getElementById('results');
  if (!q || !res || typeof window.SEARCH_INDEX === 'undefined') return;
  function render(){
    var term = q.value.trim().toLowerCase();
    res.innerHTML = '';
    if (!term) return;
    var hits = 0;
    window.SEARCH_INDEX.forEach(function(page){
      if (!page.s.toLowerCase().includes(term)) return;
      hits++;
      var i = page.s.toLowerCase().indexOf(term);
      var start = Math.max(0, i - 60), len = Math.min(page.s.length - start, 200);
      var ctx = page.s.substr(start, len);
      if (start > 0) ctx = '…' + ctx;
      var d = document.createElement('div');
      d.className = 'hit';
      d.innerHTML = '<a href="' + page.u + '">' + page.t + '</a>' +
        '<span style="color:var(--text-faint);font-size:12px;margin-left:8px">' + page.g + '</span>' +
        '<p>' + ctx.replace(new RegExp('(' + term.replace(/[.*+?^${}()|[\\]\\\\]/g, '\\\\$&') + ')', 'gi'), '<mark>$1</mark>') + '</p>';
      res.appendChild(d);
    });
    if (!hits) res.innerHTML = '<p style="color:var(--text-faint)">Nada encontrado para “' + escHtml(term) + '”.</p>';
  }
  function escHtml(s){ return s.replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;'); }
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
