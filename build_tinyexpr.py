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

# Compile units_bridge.c for native test environment
if env.get("PIOENV") == "native":
    env.BuildSources(
        os.path.join("$BUILD_DIR", "bridge"),
        os.path.join(project_dir, "src"),
        src_filter=[
            "+<units_bridge.c>"
        ]
    )

# Rename the output binary
env.Replace(PROGNAME="cardulator")

