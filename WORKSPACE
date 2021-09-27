load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

http_archive(
    name = "astc-encoder",
    build_file_content = """
cc_library(
    name = "astc-encoder",
    srcs = glob(["Source/*.cpp", "Source/*.h"], exclude=["Source/*toplevel*.cpp"]),
    includes = ["Source"],
    defines = ["NDEBUG"],
    visibility = ["//visibility:public"],
)
""",
    strip_prefix = "astc-encoder-3.2",
    urls = ["https://github.com/ARM-software/astc-encoder/archive/refs/tags/3.2.zip"],
)
