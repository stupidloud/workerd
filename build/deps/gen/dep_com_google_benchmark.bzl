# WARNING: THIS FILE IS AUTOGENERATED BY update-deps.py DO NOT EDIT

load("@//:build/http.bzl", "http_archive")

TAG_NAME = "v1.9.4"
URL = "https://api.github.com/repos/google/benchmark/tarball/v1.9.4"
STRIP_PREFIX = "google-benchmark-31c66f4"
SHA256 = "5623f21dbb25028121c49b27a780c9bd688a866278cdbed0daf455330a183f03"
TYPE = "tgz"

def dep_com_google_benchmark():
    http_archive(
        name = "com_google_benchmark",
        url = URL,
        strip_prefix = STRIP_PREFIX,
        type = TYPE,
        sha256 = SHA256,
    )
