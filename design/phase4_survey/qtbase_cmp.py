import os, re, sys
U = "/Users/simmers/Library/Caches/bazel/_bazel_simmers/223ccc89642a35e03547a0f3440b8160/external/qtbase+"
O = "/Users/simmers/development/qt_bazel_workspace/modules/qtbase/6.10.2/overlay/BUILD.bazel"
build = open(O).read()
mentioned = set(re.findall(r'"([^"]+\.(?:cpp|c|cc|mm|m))"', build))
# also textual includes etc
mentioned |= set(re.findall(r'"([^"]+\.(?:h))"', build))
SRC_EXT = ('.cpp', '.c', '.cc', '.mm', '.m')

def strip_comments(t):
    return "\n".join(l.split('#')[0] if not l.strip().startswith('#') else '' for l in t.splitlines())

CALL = re.compile(r'\b(qt_internal_add_module|qt_internal_add_plugin|qt_internal_extend_target|qt_internal_add_3rdparty_library|qt_internal_add_tool|qt_internal_add_cmake_library|qt_internal_add_app)\s*\(', re.I)
KEYS = {'SOURCES','CONDITION','LIBRARIES','PUBLIC_LIBRARIES','DEFINES','INCLUDE_DIRECTORIES','PUBLIC_DEFINES','COMPILE_OPTIONS','LINK_OPTIONS','NO_UNITY_BUILD_SOURCES','PRIVATE_MODULE_INTERFACE','PUBLIC_INCLUDE_DIRECTORIES','PLUGIN_TYPE','DEFAULT_IF','CLASS_NAME','OUTPUT_NAME','QMAKE_LIB_NAME','NO_UNITY_BUILD','ATTRIBUTION_FILE','SBOM_INCOMPLETE_3RD_PARTY_DEPENDENCIES','EXTRA_CMAKE_FILES','EXTRA_CMAKE_INCLUDES','PRECOMPILED_HEADER','NO_PCH_SOURCES','MOC_OPTIONS','DISABLE_AUTOGEN_TOOLS','ENABLE_AUTOGEN_TOOLS','PUBLIC_COMPILE_OPTIONS','GENERATE_CPP_EXPORTS','EXCEPTIONS','HEADER_SYNC_SOURCE_DIRECTORY','PUBLIC_LINK_OPTIONS','EXTERNAL_HEADERS','EXTERNAL_HEADERS_DIR','QMAKE_MODULE_CONFIG','CONFIG_MODULE_NAME','STATIC','INSTALL','SKIP_INSTALL','MODULE_INCLUDE_NAME','NO_GENERATE_CPP_EXPORTS','INSTALL_DIR','TARGET_DESCRIPTION','TOOLS_TARGET','NO_CONFIG_HEADER_FILE','INTERNAL_MODULE','HEADER_MODULE','NO_PRIVATE_MODULE','NO_MODULE_HEADERS','QT_LICENSE_ID','CPE','PURL','CPE_VALUE','PURL_VALUE','NO_PCH','QMAKE_MODULE_CONFIG_PRIVATE','PLUGIN_TYPE','EXTRA_LIBRARY_DIRS','NO_SYNC_QT','ALLOW_UNDEFINED_SYMBOLS','NO_UNITY_BUILD','SBOM_DEPENDENCIES','ATTRIBUTION_ENTRY_INDEX','COMPILE_FLAGS','OPTIONS'}

def parse_calls(text):
    out = []
    for m in CALL.finditer(text):
        i = m.end(); depth = 1; j = i
        while depth and j < len(text):
            if text[j] == '(': depth += 1
            elif text[j] == ')': depth -= 1
            j += 1
        body = text[i:j-1]
        toks = re.findall(r'"[^"]*"|\S+', body)
        target = toks[0] if toks else ''
        cur = None; d = {}
        for t in toks[1:]:
            if t in KEYS or (t.isupper() and re.match(r'^[A-Z_]+$', t) and t not in ('AND','OR','NOT','STREQUAL','TARGET','EQUAL','MATCHES','VERSION_LESS','VERSION_GREATER','DEFINED')):
                cur = t; d.setdefault(cur, []); continue
            if cur: d[cur].append(t)
        out.append((m.group(1), target, d, text[:m.start()].count('\n')+1))
    return out

skip = ('/tests/', '/doc/', '/examples/', 'src/android', 'wasm', '/ios', 'qnx', 'integrity', 'vxworks', 'cmake/tests')
for root, dirs, files in os.walk(os.path.join(U, 'src')):
    if 'CMakeLists.txt' not in files: continue
    rel = os.path.relpath(root, U)
    if any(s in '/' + rel + '/' for s in skip): continue
    p = os.path.join(root, 'CMakeLists.txt')
    text = strip_comments(open(p).read())
    calls = parse_calls(text)
    for kind, tgt, d, line in calls:
        srcs = [s for s in d.get('SOURCES', []) + d.get('NO_PCH_SOURCES', []) if s.endswith(SRC_EXT)]
        if not srcs: continue
        missing = []
        for s in srcs:
            s2 = s.strip('"')
            if '$' in s2: continue
            path = os.path.normpath(os.path.join(rel, s2))
            if path not in mentioned:
                missing.append(s2)
        if missing:
            cond = ' '.join(d.get('CONDITION', [])) or '(unconditional)'
            print(f"{rel}/CMakeLists.txt:{line} {kind} {tgt} COND[{cond}] missing {len(missing)}/{len(srcs)}: {' '.join(missing[:12])}{' ...' if len(missing)>12 else ''}")
