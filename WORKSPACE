workspace(name = "envoy")

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

load("//bazel:api_binding.bzl", "envoy_api_binding")

envoy_api_binding()

load("//bazel:api_repositories.bzl", "envoy_api_dependencies")

envoy_api_dependencies()

load("//bazel:repositories.bzl", "envoy_dependencies")

envoy_dependencies()

load("//bazel:dependency_imports.bzl", "envoy_dependency_imports")

envoy_dependency_imports()

http_archive(
    name = "rules_antlr",
    urls = ["https://github.com/marcohu/rules_antlr/archive/3cc2f9502a54ceb7b79b37383316b23c4da66f9a.tar.gz"],
    strip_prefix = "rules_antlr-3cc2f9502a54ceb7b79b37383316b23c4da66f9a",
    sha256 = "7249d1569293d9b239e23c65f6b4c81a07da921738bde0dfeb231ed98be40429",
)

load("@rules_antlr//antlr:deps.bzl", "antlr_dependencies")
antlr_dependencies(472)

http_archive(
    name = "antlr4_runtimes",
    sha256 = "4d0714f441333a63e50031c9e8e4890c78f3d21e053d46416949803e122a6574",
    strip_prefix = "antlr4-4.7.1",
    urls = ["https://github.com/antlr/antlr4/archive/4.7.1.tar.gz"],
    build_file_content = """
package(default_visibility = ["//visibility:public"])
cc_library(
    name = "cpp",
    srcs = glob(["runtime/Cpp/runtime/src/**/*.cpp"]),
    hdrs = glob(["runtime/Cpp/runtime/src/**/*.h"]),
    includes = ["runtime/Cpp/runtime/src"],
)
"""
)
