// Copyright (c) 2017-2022 Cloudflare, Inc.
// Licensed under the Apache 2.0 license found in the LICENSE file or at:
//     https://opensource.org/licenses/Apache-2.0

#include "pyodide.h"

#include <kj/test.h>

namespace workerd::api {
namespace {

KJ_TEST("getPythonSnapshotRelease") {
  capnp::MallocMessageBuilder arena;
  // TODO(beta): Factor out FeatureFlags from WorkerBundle.
  auto featureFlags = arena.initRoot<CompatibilityFlags>();

  {
    auto res = getPythonSnapshotRelease(featureFlags);
    KJ_ASSERT(res == kj::none);
  }

  featureFlags.setPythonWorkers(true);
  {
    auto res = KJ_ASSERT_NONNULL(getPythonSnapshotRelease(featureFlags));
    KJ_ASSERT(res.getPyodide() == "0.26.0a2");
    KJ_ASSERT(res.getFlagName() == "pythonWorkers");
  }

  featureFlags.setPythonWorkersDevPyodide(true);
  {
    auto res = KJ_ASSERT_NONNULL(getPythonSnapshotRelease(featureFlags));
    KJ_ASSERT(res.getPyodide() == "dev");
    KJ_ASSERT(res.getFlagName() == "pythonWorkersDevPyodide");
  }

  featureFlags.setPythonWorkers(false);
  {
    auto res = KJ_ASSERT_NONNULL(getPythonSnapshotRelease(featureFlags));
    KJ_ASSERT(res.getPyodide() == "dev");
    KJ_ASSERT(res.getFlagName() == "pythonWorkersDevPyodide");
  }

  featureFlags.setPythonWorkers20250116(true);
  {
    auto res = KJ_ASSERT_NONNULL(getPythonSnapshotRelease(featureFlags));
    KJ_ASSERT(res.getPyodide() == "0.27.7");
    KJ_ASSERT(res.getFlagName() == "pythonWorkers20250116");
  }

  featureFlags.setPythonWorkersDevPyodide(false);
  {
    auto res = KJ_ASSERT_NONNULL(getPythonSnapshotRelease(featureFlags));
    KJ_ASSERT(res.getPyodide() == "0.27.7");
    KJ_ASSERT(res.getFlagName() == "pythonWorkers20250116");
  }
}

KJ_TEST("basic `import` tests") {
  auto files = kj::heapArrayBuilder<kj::String>(2);
  files.add(kj::str("import a\nimport z"));
  files.add(kj::str("import b"));
  auto result = pyodide::PythonModuleInfo::parsePythonScriptImports(files.finish());
  KJ_REQUIRE(result.size() == 3);
  KJ_REQUIRE(result[0] == "a");
  KJ_REQUIRE(result[1] == "z");
  KJ_REQUIRE(result[2] == "b");
}

KJ_TEST("supports whitespace") {
  auto files = kj::heapArrayBuilder<kj::String>(1);
  files.add(kj::str("import      a\nimport    \n\tz"));
  auto result = pyodide::PythonModuleInfo::parsePythonScriptImports(files.finish());
  KJ_REQUIRE(result.size() == 2);
  KJ_REQUIRE(result[0] == "a");
  KJ_REQUIRE(result[1] == "z");
}

KJ_TEST("supports windows newlines") {
  auto files = kj::heapArrayBuilder<kj::String>(1);
  files.add(kj::str("import      a\r\nimport    \r\n\tz"));
  auto result = pyodide::PythonModuleInfo::parsePythonScriptImports(files.finish());
  KJ_REQUIRE(result.size() == 2);
  KJ_REQUIRE(result[0] == "a");
  KJ_REQUIRE(result[1] == "z");
}

KJ_TEST("basic `from` test") {
  auto files = kj::heapArrayBuilder<kj::String>(1);
  files.add(kj::str("from x import a,b\nfrom z import y"));
  auto result = pyodide::PythonModuleInfo::parsePythonScriptImports(files.finish());
  KJ_REQUIRE(result.size() == 2);
  KJ_REQUIRE(result[0] == "x");
  KJ_REQUIRE(result[1] == "z");
}

KJ_TEST("ignores indented blocks") {
  auto files = kj::heapArrayBuilder<kj::String>(1);
  files.add(kj::str("import a\nif True:\n  import x\nimport y"));
  auto result = pyodide::PythonModuleInfo::parsePythonScriptImports(files.finish());
  KJ_REQUIRE(result.size() == 2);
  KJ_REQUIRE(result[0] == "a");
  KJ_REQUIRE(result[1] == "y");
}

KJ_TEST("supports nested imports") {
  auto files = kj::heapArrayBuilder<kj::String>(1);
  files.add(kj::str("import a.b\nimport z.x.y.i"));
  auto result = pyodide::PythonModuleInfo::parsePythonScriptImports(files.finish());
  KJ_REQUIRE(result.size() == 2);
  KJ_REQUIRE(result[0] == "a.b");
  KJ_REQUIRE(result[1] == "z.x.y.i");
}

KJ_TEST("nested `from` test") {
  auto files = kj::heapArrayBuilder<kj::String>(1);
  files.add(kj::str("from x.y.z import a,b\nfrom z import y"));
  auto result = pyodide::PythonModuleInfo::parsePythonScriptImports(files.finish());
  KJ_REQUIRE(result.size() == 2);
  KJ_REQUIRE(result[0] == "x.y.z");
  KJ_REQUIRE(result[1] == "z");
}

KJ_TEST("ignores trailing period") {
  auto files = kj::heapArrayBuilder<kj::String>(1);
  files.add(kj::str("import a.b.\nimport z.x.y.i."));
  auto result = pyodide::PythonModuleInfo::parsePythonScriptImports(files.finish());
  KJ_REQUIRE(result.size() == 0);
}

KJ_TEST("ignores relative import") {
  // This is where we diverge from the old AST-based approach. It would have returned `y` in the
  // input below.
  auto files = kj::heapArrayBuilder<kj::String>(1);
  files.add(kj::str("import .a.b\nimport ..z.x\nfrom .y import x"));
  auto result = pyodide::PythonModuleInfo::parsePythonScriptImports(files.finish());
  KJ_REQUIRE(result.size() == 0);
}

KJ_TEST("supports commas") {
  auto files = kj::heapArrayBuilder<kj::String>(1);
  files.add(kj::str("import a,b"));
  auto result = pyodide::PythonModuleInfo::parsePythonScriptImports(files.finish());
  KJ_REQUIRE(result.size() == 2);
  KJ_REQUIRE(result[0] == "a");
  KJ_REQUIRE(result[1] == "b");
}

KJ_TEST("supports backslash") {
  auto files = kj::heapArrayBuilder<kj::String>(4);
  files.add(kj::str("import a\\\n,b"));
  files.add(kj::str("import\\\n q,w"));
  files.add(kj::str("from \\\nx import y"));
  files.add(kj::str("from \\\n   c import y"));
  auto result = pyodide::PythonModuleInfo::parsePythonScriptImports(files.finish());
  KJ_REQUIRE(result.size() == 6);
  KJ_REQUIRE(result[0] == "a");
  KJ_REQUIRE(result[1] == "b");
  KJ_REQUIRE(result[2] == "q");
  KJ_REQUIRE(result[3] == "w");
  KJ_REQUIRE(result[4] == "x");
  KJ_REQUIRE(result[5] == "c");
}

KJ_TEST("multiline-strings ignored") {
  auto files = kj::heapArrayBuilder<kj::String>(4);
  files.add(kj::str(R"SCRIPT(
FOO="""
import x
from y import z
"""
)SCRIPT"));
  files.add(kj::str(R"SCRIPT(
FOO='''
import f
from g import z
'''
)SCRIPT"));
  files.add(kj::str(R"SCRIPT(FOO = "\
import b \
")SCRIPT"));
  files.add(kj::str("FOO=\"\"\"  \n", R"SCRIPT(import x
from y import z
""")SCRIPT"));
  auto result = pyodide::PythonModuleInfo::parsePythonScriptImports(files.finish());
  KJ_REQUIRE(result.size() == 0);
}

KJ_TEST("multiline-strings with imports in-between") {
  auto files = kj::heapArrayBuilder<kj::String>(1);
  files.add(kj::str(
      R"SCRIPT(FOO="""
import x
from y import z
"""import q
import w
BAR="""
import e
"""
from t import u)SCRIPT"));
  auto result = pyodide::PythonModuleInfo::parsePythonScriptImports(files.finish());
  KJ_REQUIRE(result.size() == 2);
  KJ_REQUIRE(result[0] == "w");
  KJ_REQUIRE(result[1] == "t");
}

KJ_TEST("import after string literal") {
  auto files = kj::heapArrayBuilder<kj::String>(1);
  files.add(kj::str(R"SCRIPT(import a
"import b)SCRIPT"));
  auto result = pyodide::PythonModuleInfo::parsePythonScriptImports(files.finish());
  KJ_REQUIRE(result.size() == 1);
  KJ_REQUIRE(result[0] == "a");
}

KJ_TEST("import after `i`") {
  auto files = kj::heapArrayBuilder<kj::String>(1);
  files.add(kj::str(R"SCRIPT(import a
iimport b)SCRIPT"));
  auto result = pyodide::PythonModuleInfo::parsePythonScriptImports(files.finish());
  KJ_REQUIRE(result.size() == 1);
  KJ_REQUIRE(result[0] == "a");
}

KJ_TEST("langchain import") {
  auto files = kj::heapArrayBuilder<kj::String>(1);
  files.add(kj::str(R"SCRIPT(from js import Response, console, URL
from langchain.chat_models import ChatOpenAI
import openai)SCRIPT"));
  auto result = pyodide::PythonModuleInfo::parsePythonScriptImports(files.finish());
  KJ_REQUIRE(result.size() == 3);
  KJ_REQUIRE(result[0] == "js");
  KJ_REQUIRE(result[1] == "langchain.chat_models");
  KJ_REQUIRE(result[2] == "openai");
}

KJ_TEST("quote in multiline string") {
  auto files = kj::heapArrayBuilder<kj::String>(1);
  files.add(kj::str(R"SCRIPT(temp = """
w["h
""")SCRIPT"));
  auto result = pyodide::PythonModuleInfo::parsePythonScriptImports(files.finish());
  KJ_REQUIRE(result.size() == 0);
}

using pyodide::PythonModuleInfo;

template <typename... Params>
kj::Array<kj::String> strArray(Params&&... params) {
  return kj::arr(kj::str(params)...);
}

template <typename... Params>
kj::Array<kj::Array<kj::byte>> bytesArray(Params&&... params) {
  return kj::arr(kj::heapArray<kj::byte>(kj::str(params).asBytes())...);
}

template <typename... Params>
kj::HashSet<kj::String> strSet(Params&&... params) {
  auto array = strArray(params...);
  kj::HashSet<kj::String> set;
  for (auto& str: array) {
    set.insert(kj::mv(str));
  }
  return set;
}

KJ_TEST("basic test of getPackageSnapshotImports") {
  auto a = pyodide::PythonModuleInfo(strArray("a.py"),
      bytesArray("from js import Response\n"
                 "import asyncio\n"
                 "import numbers\n"
                 "def on_fetch(request):\n"
                 "  return Response.new('Hello')\n"));
  auto result = a.getPackageSnapshotImports("0.26.0a2");
  KJ_REQUIRE(result.size() == 1);
  KJ_REQUIRE(result[0] == "numbers");
};

KJ_TEST("basic test of getPackageSnapshotImports user module") {
  auto a = pyodide::PythonModuleInfo(strArray("a.py", "numbers.py"),
      bytesArray("from js import Response\n"
                 "import asyncio\n"
                 "import numbers\n"
                 "def on_fetch(request):\n"
                 "  return Response.new('Hello')\n",
          ""));
  auto result = a.getPackageSnapshotImports("0.26.0a2");
  KJ_REQUIRE(result.size() == 0);
};

kj::Array<kj::String> filterPythonScriptImports(
    kj::Array<kj::String> names, kj::ArrayPtr<kj::String> imports, kj::StringPtr version) {
  auto contentsBuilder = kj::heapArrayBuilder<kj::Array<kj::byte>>(names.size());
  for (auto _: kj::zeroTo(names.size())) {
    (void)_;
    contentsBuilder.add(kj::Array<kj::byte>(0));
  }
  auto modInfo = pyodide::PythonModuleInfo(kj::mv(names), contentsBuilder.finish());
  auto modSet = modInfo.getWorkerModuleSet();
  return PythonModuleInfo::filterPythonScriptImports(kj::mv(modSet), kj::mv(imports), version);
}

KJ_TEST("Simple pass through") {
  auto imports = strArray("b", "c");
  auto result = filterPythonScriptImports({}, kj::mv(imports), "");
  KJ_REQUIRE(result.size() == 2);
  KJ_REQUIRE(result[0] == "b");
  KJ_REQUIRE(result[1] == "c");
}

KJ_TEST("pyodide and submodules") {
  auto imports = strArray("pyodide", "pyodide.ffi");
  auto result = filterPythonScriptImports({}, kj::mv(imports), "0.26.0a2");
  KJ_REQUIRE(result.size() == 0);
}

KJ_TEST("js and submodules") {
  auto imports = strArray("js", "js.crypto");
  auto result = filterPythonScriptImports({}, kj::mv(imports), "0.26.0a2");
  KJ_REQUIRE(result.size() == 0);
}

KJ_TEST("importlib and submodules") {
  // importlib and importlib.metadata are imported into the baseline snapshot, but importlib.resources is not.
  auto imports = strArray("importlib", "importlib.metadata", "importlib.resources");
  auto result = filterPythonScriptImports({}, kj::mv(imports), "");
  KJ_REQUIRE(result.size() == 1);
  KJ_REQUIRE(result[0] == "importlib.resources");
}

KJ_TEST("Filter worker .py files") {
  auto workerModules = strArray("b.py", "c.py");
  auto imports = strArray("b", "c", "d");
  auto result = filterPythonScriptImports(kj::mv(workerModules), kj::mv(imports), "");
  KJ_REQUIRE(result.size() == 1);
  KJ_REQUIRE(result[0] == "d");
}

KJ_TEST("Filter worker module/__init__.py") {
  auto workerModules = strArray("a/__init__.py", "b/__init__.py", "c/a.py");
  auto imports = strArray("a", "b", "c");
  auto result = filterPythonScriptImports(kj::mv(workerModules), kj::mv(imports), "");
  KJ_REQUIRE(result.size() == 0);
}

KJ_TEST("Filters out subdir/submodule") {
  auto workerModules = strArray("subdir/submodule.py");
  auto imports = strArray("subdir.submodule");
  auto result = filterPythonScriptImports(kj::mv(workerModules), kj::mv(imports), "");
  KJ_REQUIRE(result.size() == 0);
}

KJ_TEST("Filters out so") {
  auto workerModules = strArray("a.so", "b.txt");
  auto imports = strArray("a", "b");
  auto result = filterPythonScriptImports(kj::mv(workerModules), kj::mv(imports), "");
  KJ_REQUIRE(result.size() == 1);
  KJ_REQUIRE(result[0] == "b");
}

KJ_TEST("Filters out vendor stuff") {
  auto workerModules = strArray("python_modules/a.py", "python_modules/package/b.py",
      "python_modules/c.so", "python_modules/x.txt");
  auto imports = strArray("a", "package", "x");
  auto result = filterPythonScriptImports(kj::mv(workerModules), kj::mv(imports), "");
  KJ_REQUIRE(result.size() == 1);
  KJ_REQUIRE(result[0] == "x");
}

}  // namespace
}  // namespace workerd::api
