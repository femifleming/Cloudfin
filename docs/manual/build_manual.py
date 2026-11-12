#!/usr/bin/env python3
"""Builds "Cloudfin Manual.pdf" from ../HELP.md: turns the Markdown into a page and prints it with
Chrome (run it from anywhere; it needs Google Chrome or Chromium)."""
import html, os, re, shutil, subprocess, sys

here = os.path.dirname(os.path.abspath(__file__))
docs = os.path.dirname(here)
md = open(os.path.join(docs, "HELP.md"), encoding="utf-8").read()

def inline(t):
    t = html.escape(t, quote=False)
    t = re.sub(r"!\[([^\]]*)\]\(([^)]+)\)", lambda m: '<img src="../%s" alt="%s">' % (m.group(2), m.group(1)), t)
    t = re.sub(r"\[([^\]]+)\]\((#[^)]+)\)", r"\1", t)                       # links inside the page: just the words
    t = re.sub(r"\[([^\]]+)\]\(([^)]+)\)", r'<a href="\2">\1</a>', t)
    t = re.sub(r"\*\*([^*]+)\*\*", r"<b>\1</b>", t)
    t = re.sub(r"\*([^*]+)\*", r"<i>\1</i>", t)
    return t

out, in_list, in_contents = [], False, False
for line in md.split("\n"):
    if line.startswith("## Contents"): in_contents = True; continue        # (the PDF has no use for the link list)
    if in_contents:
        if line.startswith("## "): in_contents = False
        else: continue
    if line.startswith("- "):
        if not in_list: out.append("<ul>"); in_list = True
        out.append("<li>%s</li>" % inline(line[2:])); continue
    if in_list: out.append("</ul>"); in_list = False
    if line.startswith("# "):    out.append('<h1>%s</h1>' % inline(line[2:].replace(": help", "")))
    elif line.startswith("## "): out.append('<h2>%s</h2>' % inline(line[3:]))
    elif line.strip():           out.append('<p>%s</p>' % inline(line))
if in_list: out.append("</ul>")

page = """<!doctype html><html lang="en"><head><meta charset="utf-8"><title>Cloudfin Manual</title><style>
@page{size:A4;margin:18mm 17mm}
body{font:10.6pt/1.5 "Avenir Next","Segoe UI",Helvetica,Arial,sans-serif;color:#16294a}
h1{font:700 30pt "Arial Rounded MT Bold","Avenir Next",sans-serif;color:#586895;margin:0 0 4mm;text-align:center}
h2{font-size:14pt;color:#586895;border-bottom:1.2pt solid #92a0c1;padding-bottom:2pt;margin:9mm 0 3mm;break-after:avoid}
p{margin:0 0 7pt} img{max-width:100%;border-radius:4pt;margin:3pt 0} ul{margin:0 0 8pt;padding-left:16pt}
.sub{text-align:center;color:#7580a8;margin-bottom:8mm}
</style></head><body>""" + "\n".join(out).replace("</h1>", '</h1><p class="sub">Manual, version %s</p>' % (sys.argv[1] if len(sys.argv) > 1 else ""), 1) + "</body></html>"
src = os.path.join(here, "manual.html")
open(src, "w", encoding="utf-8").write(page)
chrome = next((c for c in ("/Applications/Google Chrome.app/Contents/MacOS/Google Chrome", shutil.which("google-chrome"), shutil.which("chromium")) if c and os.path.exists(c)), None)
if chrome is None: sys.exit("Chrome not found: open manual.html and print it to PDF by hand")
subprocess.run([chrome, "--headless", "--disable-gpu", "--no-pdf-header-footer", "--print-to-pdf=" + os.path.join(here, "Cloudfin Manual.pdf"), "file://" + src],
               check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
print("wrote", os.path.join(here, "Cloudfin Manual.pdf"))
