import os
Import("env")

project_dir = env.get("PROJECT_DIR")
tinyexpr_dir = os.path.join(project_dir, "lib", "tinyexpr-plusplus")

# Add include path to the build environment
env.Append(CPPPATH=[
    tinyexpr_dir
])

# Compile tinyexpr sources and add them to the build
env.BuildSources(
    os.path.join("$BUILD_DIR", "tinyexpr"),
    tinyexpr_dir,
    src_filter=[
        "+<tinyexpr.cpp>"
    ]
)

# Rename the output binary
env.Replace(PROGNAME="cardulator")
