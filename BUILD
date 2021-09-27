cc_library(
    name = "astc_pdr",
    srcs = [
        "astc_pdr.cpp",
        "astc_pdr.h",
    ],
    copts = [
        "-pthread",
        "-static-libstdc++",
        "-DNDEBUG",
    ],
    deps = ["@astc-encoder"],
)

cc_binary(
    name = "astcpdr-test",
    srcs = [
        "astc_pdr_test.cpp",
    ],
    linkopts = [
        "-pthread",
    ],
    deps = [
        ":astc_pdr",
        "@astc-encoder",
    ],
)
