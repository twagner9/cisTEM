#!/usr/bin/env python3
"""
Add GetInteractiveParameters() implementations to cpp program files that
define classes derived from MyApp, and try to populate parameter entries
from DoInteractiveUserInput() bodies by parsing Get*FromUser() calls.
Also inserts the declaration inside the class definition.
Usage (from repo root):
  mkdir -p scripts
  (save this file as scripts/add_interactive_parameters.py)
  chmod +x scripts/add_interactive_parameters.py
  ./scripts/add_interactive_parameters.py
"""
import re, shutil, subprocess
from pathlib import Path

def git_root():
    p = subprocess.run(["git","rev-parse","--show-toplevel"], capture_output=True, text=True)
    if p.returncode != 0:
        raise SystemExit("Not in a git repo or git not available.")
    return Path(p.stdout.strip())

ROOT = git_root()
SRC  = ROOT / "src" / "programs"
LOG  = ROOT / "scripts" / "add_interactive_parameters.log"
BACKUP_SUFFIX = ".bak_interactive_params"

if not SRC.exists():
    print("Directory not found:", SRC)
    raise SystemExit(1)

# ── class declaration ────────────────────────────────────────────────────────
class_re = re.compile(r'\bclass\s+([A-Za-z0-9_]+)\s*:\s*public\s+MyApp\b')

# ── DoInteractiveUserInput body extractor ────────────────────────────────────
doi_start_re = re.compile(r'\bDoInteractiveUserInput\s*\(\s*\)\s*\{')

def extract_doi_body(text):
    """Return the text inside the outermost braces of DoInteractiveUserInput()
    or None if not found."""
    m = doi_start_re.search(text)
    if not m:
        return None
    depth = 1
    i = m.end()
    while i < len(text) and depth:
        c = text[i]
        if c == '{':
            depth += 1
        elif c == '}':
            depth -= 1
        i += 1
    return text[m.end():i - 1]

# ── Per-getter patterns ──────────────────────────────────────────────────────
_SQ  = r'"([^"]*)"'
_SEP = r'\s*,\s*'
_TOK = r'([A-Za-z0-9_:\.\+\-eE]+)'

getter_patterns = [
    ("filename", re.compile(
        r'GetFilenameFromUser\s*\(\s*' + _SQ + _SEP + _SQ + _SEP + _SQ + r'\s*,'
    )),
    ("string",   re.compile(
        r'GetStringFromUser\s*\(\s*'   + _SQ + _SEP + _SQ + _SEP + _SQ + r'\s*[\),]'
    )),
    ("float",    re.compile(
        r'GetFloatFromUser\s*\(\s*'    + _SQ + _SEP + _SQ + _SEP + _SQ + r'\s*[\),]'
    )),
    ("integer",  re.compile(
        r'GetInt(?:egerFromUser|FromUser)\s*\(\s*' + _SQ + _SEP + _SQ + _SEP + _SQ + r'\s*[\),]'
    )),
    ("yesno",    re.compile(
        r'GetYesNoFromUser\s*\(\s*'    + _SQ + _SEP + _SQ + _SEP + _SQ + r'\s*[\),]'
    )),
]

def extract_params(text):
    """Extract (name, description, default, type) tuples from a DoInteractiveUserInput body."""
    body = extract_doi_body(text)
    if body is None:
        return []
    hits = []
    for ptype, rx in getter_patterns:
        for m in rx.finditer(body):
            hits.append((m.start(), ptype, m.group(1), m.group(2), m.group(3)))
    hits.sort(key=lambda x: x[0])
    return [(name, desc, dflt, ptype) for (_, ptype, name, desc, dflt) in hits]

# ── Class declaration insertion ──────────────────────────────────────────────
# NEW: matches the DoInteractiveUserInput *declaration* line inside the class
# body, capturing its leading whitespace so we can mirror the indentation.
doi_decl_re = re.compile(
    r'([ \t]*)void\s+DoInteractiveUserInput\s*\([^)]*\)\s*;[ \t]*'
)

DECL = "std::vector<MyApp::InteractiveParameter> GetInteractiveParameters( ) const override;"

def has_declaration(text):
    return "GetInteractiveParameters" in text

def insert_declaration(text):
    """Insert the GetInteractiveParameters declaration on the line immediately
    after void DoInteractiveUserInput(...); inside the class body, matching
    its indentation.  Returns (new_text, success)."""
    m = doi_decl_re.search(text)
    if not m:
        return text, False
    indent     = m.group(1)           # e.g. four spaces or a tab
    insert_pos = m.end()              # right after the declaration line
    new_line   = f"\n{indent}{DECL}"
    return text[:insert_pos] + new_line + text[insert_pos:], True

# ── Code-generation helpers ──────────────────────────────────────────────────
method_template = """\

// Auto-added by scripts/add_interactive_parameters.py
std::vector<MyApp::InteractiveParameter> {cls}::GetInteractiveParameters() const {{
    std::vector<MyApp::InteractiveParameter> params;
{body}
    return params;
}}
"""

def _esc(s):
    return s.replace('\\', '\\\\').replace('"', '\\"')


def build_body(params):
    if not params:
        return '    (void)params; // no parameters detected\n'
    lines = []
    for (name, desc, dflt, ptype) in params:
        lines.append(
            '    params.push_back( MyApp::InteractiveParameter{{'
            '"{name}", "{desc}", "{dflt}"'
            '}} );\n'.format(
                name=_esc(name),
                desc=_esc(desc),
                dflt=_esc(dflt),
            )
        )
    return "".join(lines)

# ── File mutation ────────────────────────────────────────────────────────────
def has_definition(text):
    return "GetInteractiveParameters()" in text

def modify_file(path, cls, params):
    text = path.read_text()

    need_decl = not has_declaration(text)
    need_defn = not has_definition(text)

    if not need_decl and not need_defn:
        return False, "already_has_both"

    # Take a single backup before any edits.
    bkp = path.with_name(path.name + BACKUP_SUFFIX)
    shutil.copy2(path, bkp)

    actions = []

    # NEW: insert the declaration inside the class body first, so the
    # has_definition guard above never interferes with the text we're about
    # to append.
    if need_decl:
        text, ok = insert_declaration(text)
        if ok:
            actions.append("decl_inserted")
        else:
            actions.append("decl_not_found")

    if need_defn:
        body   = build_body(params)
        method = method_template.format(cls=cls, body=body)
        text   = text.rstrip() + "\n" + method + "\n"
        actions.append("defn_inserted")

    path.write_text(text)
    return True, "+".join(actions)

# ── Main ─────────────────────────────────────────────────────────────────────
def main():
    LOG.parent.mkdir(parents=True, exist_ok=True)
    with LOG.open("w") as log:
        log.write("Run in: {}\n".format(ROOT))

    changed = []
    for cpp in sorted(SRC.rglob("*.cpp")):
        txt = cpp.read_text(errors="ignore")
        m   = class_re.search(txt)
        if not m:
            continue
        cls    = m.group(1)
        params = extract_params(txt)
        ok, msg = modify_file(cpp, cls, params)
        with LOG.open("a") as log:
            log.write(f"{cpp}: class={cls} => {msg}, params_found={len(params)}\n")
            for (name, desc, dflt, ptype) in params:
                log.write(f"  [{ptype}] name={name!r}  desc={desc!r}  default={dflt!r}\n")
        if ok:
            changed.append((cpp, cls, len(params)))
            print(f"Updated  {cpp}  class={cls}  params={len(params)}  ({msg})")

    if not changed:
        print("No files changed. See log:", LOG)
    else:
        print(f"\nModified {len(changed)} file(s). Backups: *{BACKUP_SUFFIX}")
        print("Log:", LOG)

if __name__ == "__main__":
    main()