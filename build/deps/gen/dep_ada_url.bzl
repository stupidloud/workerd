# WARNING: THIS FILE IS AUTOGENERATED BY update-deps.py DO NOT EDIT

load("@//:build/http.bzl", "http_archive")

TAG_NAME = "v3.2.4"
URL = "https://github.com/ada-url/ada/releases/download/v3.2.4/singleheader.zip"
STRIP_PREFIX = ""
SHA256 = "bd89fcf57c93e965e6e2488448ab9d1cf8005311808c563b288f921d987e4924"
TYPE = "zip"

def dep_ada_url():
    http_archive(
        name = "ada-url",
        url = URL,
        strip_prefix = STRIP_PREFIX,
        type = TYPE,
        sha256 = SHA256,
        build_file = "//:build/BUILD.ada-url",
    )
