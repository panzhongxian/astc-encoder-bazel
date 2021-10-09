workspace(name = "io_opencensus_cpp")

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

http_archive(
    name = "astc-encoder",
    build_file_content = """
load("@io_opencensus_cpp//:copts.bzl", "ASTC_ENCODER_COPTS_AVX2")	
cc_library(
    name = "astc-encoder",
    srcs = glob(["Source/*.cpp", "Source/*.h"], exclude=["Source/*toplevel*.cpp"]),
    includes = ["Source"],
    copts = ASTC_ENCODER_COPTS_AVX2 + [
        "-O3",
        "-DNDEBUG",
    ],
    visibility = ["//visibility:public"],
)
""",
    sha256 = "ceeaec72fd7b2313d8e3d41d3a93bc3c16f98c0191ba9c557e0fb6307221f564",
    strip_prefix = "astc-encoder-3.2",
    urls = ["https://github.com/ARM-software/astc-encoder/archive/refs/tags/3.2.zip"],
)
