cc_library(
    name = "astc_wrapper",
    srcs = [
        "astc_wrapper.cpp",
        "astc_wrapper.h",
    ],
    copts = [
        "-pthread",
        "-static-libstdc++",
        "-DNDEBUG",
    ],
    visibility = [
        "//test:__subpackages__",
    ],
    deps = ["@astc-encoder"],
)
