#!/usr/bin/env python3
"""Compile the actual asynchronous write callback against a minimal C++ test double.

This is a status-reporting regression test, NOT a real foobar2000 host test.
It does not establish callback thread affinity or metadata write safety.
"""
import argparse
from pathlib import Path
import shutil
import subprocess
import tempfile


def extract_callback(source: str) -> str:
    signature = "void write_tags_completion::on_completion(unsigned p_code)"
    if source.count(signature) != 1:
        raise AssertionError("Expected exactly one write completion callback")
    start = source.index(signature)
    opening = source.find("{", start + len(signature))
    if opening < 0:
        raise AssertionError("Missing callback body")
    depth = 0
    for pos in range(opening, len(source)):
        if source[pos] == "{":
            depth += 1
        elif source[pos] == "}":
            depth -= 1
            if depth == 0:
                return source[start:pos + 1]
    raise AssertionError("Unbalanced callback body")


def build_harness(source: str) -> str:
    callback = extract_callback(source)
    return r'''#include <cassert>
#include <initializer_list>
#include <string>
struct metadb_io { static constexpr unsigned load_info_success = 0; };
class write_tags_completion {
public:
    int errors = 0;
    int displayed = 0;
    std::string message;
    void add_error(const char* text) { ++errors; message = text; }
    void display_errors() { if (errors > 0) ++displayed; }
    void on_completion(unsigned p_code);
};
''' + callback + r'''
int main() {
    write_tags_completion success;
    success.on_completion(metadb_io::load_info_success);
    assert(success.errors == 0 && success.displayed == 0);
    for (unsigned code : {1u, 2u, 429u}) {
        write_tags_completion failure;
        failure.on_completion(code);
        assert(failure.errors == 1);
        assert(failure.displayed == 1);
        assert(failure.message == "Error writing tags.");
    }
    return 0;
}
'''


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--emit-cpp", type=Path, help="Emit C++ harness for MSVC")
    parser.add_argument("--source", type=Path, default=Path(__file__).resolve().parents[1] / "file_info_manager.cpp")
    args = parser.parse_args()
    harness = build_harness(args.source.read_text(encoding="utf-8-sig"))
    if args.emit_cpp:
        args.emit_cpp.write_text(harness, encoding="utf-8")
        print(f"Generated write callback harness: {args.emit_cpp}")
        return
    compiler = shutil.which("g++") or shutil.which("clang++")
    if not compiler:
        raise RuntimeError("No C++ compiler; use --emit-cpp for MSVC")
    with tempfile.TemporaryDirectory(prefix="discogger-write-callback-") as folder:
        src = Path(folder) / "callback_test.cpp"
        exe = Path(folder) / "callback_test"
        src.write_text(harness, encoding="utf-8")
        subprocess.run([compiler, "-std=c++17", "-Wall", "-Wextra", "-Werror", "-pedantic", str(src), "-o", str(exe)], check=True)
        subprocess.run([str(exe)], check=True)
    print("PASS: write callback success/failure behavior against mock SDK")


if __name__ == "__main__":
    main()
