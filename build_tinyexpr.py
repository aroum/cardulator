import os
Import("env")

project_dir = env.get("PROJECT_DIR")
tinyexpr_dir = os.path.join(project_dir, "lib", "tinyexpr-plusplus")
gnu_units_dir = os.path.join(project_dir, "lib", "gnu-units")

# Add include paths to the build environment
env.Append(CPPPATH=[
    tinyexpr_dir,
    gnu_units_dir
])

# Add -Dmain=gnu_units_main only to pure C compiler flags (CFLAGS), so C++ test/main.cpp remains main
env.Append(CFLAGS=[
    "-Dmain=gnu_units_main",
    "-w"
])

# Compile tinyexpr sources and add them to the build
env.BuildSources(
    os.path.join("$BUILD_DIR", "tinyexpr"),
    tinyexpr_dir,
    src_filter=[
        "+<tinyexpr.cpp>"
    ]
)

# Compile gnu-units C sources and add them to the build
env.BuildSources(
    os.path.join("$BUILD_DIR", "gnu-units"),
    gnu_units_dir,
    src_filter=[
        "+<units.c>",
        "+<parse.tab.c>",
        "+<strfunc.c>"
    ]
)

# Ensure embedded_units_dat.h and embedded_units_dat.c exist and are up to date
units_dat_src = os.path.join(gnu_units_dir, "units.dat")
embedded_c = os.path.join(project_dir, "src", "embedded_units_dat.c")
embedded_h = os.path.join(project_dir, "src", "embedded_units_dat.h")

if os.path.exists(units_dat_src) and (not os.path.exists(embedded_c) or os.path.getmtime(units_dat_src) > os.path.getmtime(embedded_c)):
    with open(units_dat_src, "rb") as f:
        lines = f.readlines()
    clean_lines = [l for l in lines if l.strip() and not l.strip().startswith(b"#")]
    data = b"".join(clean_lines)

    with open(embedded_h, "w") as h:
        h.write('#pragma once\n#include <stddef.h>\n\n#ifdef __cplusplus\nextern "C" {\n#endif\n\nextern const char embedded_units_dat[];\nextern const size_t embedded_units_dat_len;\n\n#ifdef __cplusplus\n}\n#endif\n')

    with open(embedded_c, "w") as out:
        out.write('/* Auto-generated embedded units.dat data for Cardulator */\n#include "embedded_units_dat.h"\n\nconst char embedded_units_dat[] = {\n')
        for i in range(0, len(data), 20):
            chunk = data[i:i+20]
            out.write("    " + ", ".join(str(b) for b in chunk) + ",\n")
        out.write(f"    0\n}};\n\nconst size_t embedded_units_dat_len = {len(data)};\n")

# Compile units_bridge.c and embedded_units_dat.c for native test environment
if env.get("PIOENV") == "native":
    env.BuildSources(
        os.path.join("$BUILD_DIR", "bridge"),
        os.path.join(project_dir, "src"),
        src_filter=[
            "+<units_bridge.c>",
            "+<embedded_units_dat.c>"
        ]
    )

# Rename the output binary
env.Replace(PROGNAME="cardulator")

