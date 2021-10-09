cc_binary(
    name = "astc_wrapper_test",
    srcs = [
        "astc_wrapper_test.cpp",
    ],
    data = [
        "//images",
    ],
    linkopts = [
        "-pthread",
    ],
    deps = [
        "//src:astc_wrapper",
        "@astc-encoder",
    ],
)
