import html.parser, pathlib, sys
class P(html.parser.HTMLParser):
    def __init__(s): super().__init__(); s.rows=[]; s.row=None; s.cell=None
    def handle_starttag(s,t,a):
        if t=="tr": s.row=[]
        elif t in("td","th") and s.row is not None: s.cell=[]
        elif t=="br" and s.cell is not None: s.cell.append(" / ")
    def handle_endtag(s,t):
        if t in("td","th") and s.cell is not None: s.row.append("".join(s.cell).strip()); s.cell=None
        elif t=="tr" and s.row is not None:
            r=s.row[1:] if s.row and s.row[0].isdigit() else s.row  # drop row-number column
            while r and not r[-1]: r.pop()
            if any(r): s.rows.append(r)
            s.row=None
    def handle_data(s,d):
        if s.cell is not None: s.cell.append(d)
src, out = pathlib.Path(sys.argv[1]), pathlib.Path(sys.argv[2])
for f in sorted(src.iterdir()):
    if f.is_dir(): continue
    p=P(); p.feed(f.read_text(encoding="utf-8", errors="replace"))
    name=f.stem if f.suffix==".html" else f.name
    (out/(name+".tsv")).write_text("\n".join("\t".join(r) for r in p.rows), encoding="utf-8")
    print(f"{len(p.rows):5} rows  {name}")
