# WARNING: THIS FILE IS AUTOGENERATED BY update-deps.py DO NOT EDIT

load("@//:build/http.bzl", "http_archive")

TAG_NAME = "0.11.12"
URL = "https://github.com/astral-sh/ruff/releases/download/0.11.12/ruff-aarch64-unknown-linux-gnu.tar.gz"
STRIP_PREFIX = "ruff-aarch64-unknown-linux-gnu"
SHA256 = "a14bf81d237e10abd7fc751cf55249cef10e7952a4267fb9425296e12132f2f2"
TYPE = "tgz"

def dep_ruff_linux_arm64():
    http_archive(
        name = "ruff-linux-arm64",
        url = URL,
        strip_prefix = STRIP_PREFIX,
        type = TYPE,
        sha256 = SHA256,
        build_file_content = "filegroup(name='file', srcs=['ruff'], visibility=['//visibility:public'])",
    )
