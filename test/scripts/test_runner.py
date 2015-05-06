#!/usr/bin/env python3

import argparse
import os
import sys
import subprocess
import time
import multiprocessing

DEF_RUNTIME_DIR = "./test/runtime"
HEADER_STR = "//test"
TEMP_DIR = "/tmp/ccc/"
LLVM_SUFFIX = "ll"
TIMEOUT = 5
CC = "cc"
CLANG = "clang"

DEV_NULL = open(os.devnull, 'w')

runtime_dir = DEF_RUNTIME_DIR
verbose = False
llvm = False
compiler_opts = None

description = """tests must be a c source file with a correct first line header:
//test [return n | error | exception ]"""

arg_parse = argparse.ArgumentParser(
    formatter_class=argparse.RawDescriptionHelpFormatter,
    description=description)

arg_parse.add_argument("-v", "--verbose",
                       help="Prints pass tests and compiler's output",
                       action="store_true")
arg_parse.add_argument("-l", "--llvm",
                       help="Specifies compiler command creates llvm ir",
                       action="store_true")
arg_parse.add_argument("-j", "--jobs", type=int, default=1,
                       help="Number of jobs to run in parallel")
arg_parse.add_argument("-r", "--runtime",
                       help="Alternate test runtime to use")

arg_parse.add_argument("compiler", help="Compiler and options")
arg_parse.add_argument("test", nargs="+", help="Test files")

def fail(src_path, msg):
    print("FAIL: " + src_path + " " + msg)
    return 0

def success(src_path):
    if verbose:
        print("PASS: " + src_path)
    return 1

def main():
    global verbose
    global llvm
    global compiler_opts
    global runtime_dir

    args = arg_parse.parse_args()

    if args.verbose:
        verbose = True
    if args.llvm:
        llvm = True
    if args.runtime:
        runtime_dir = args.runtime
    jobs = args.jobs

    compiler_opts = args.compiler.split()
    src_files = args.test

    if not os.path.exists(TEMP_DIR):
        os.makedirs(TEMP_DIR)

    num_tests = len(src_files)
    passed_tests = 0

    if jobs == 1:
        for src_path in src_files:
            passed_tests += process_file(src_path)
    else:
        pool = multiprocessing.Pool(processes=jobs)
        passed_tests = sum(pool.map(process_file, src_files))

    if num_tests > 1:
        sys.stderr.write("Results: Passed %d/%d\n" % (passed_tests, num_tests))

    if passed_tests != num_tests:
        sys.exit(1)

def process_file(src_path):
    temp_files = []
    runtime = os.path.join(runtime_dir, "runtime.c")

    try:
        src = open(src_path)
        header = src.readline()
        src.close()
        src_name = os.path.splitext(os.path.basename(src_path))[0]

        header = header[:len(header) - 1] # Remove trailing newline
        header = header.split(" ")

        if len(header) < 2 or header[0] != HEADER_STR:
            return fail(src_path, "Invalid header")

        is_return = False
        return_val = 0
        is_error = False
        is_except = False

        if header[1] == "return":
            if len(header) < 3:
                return fail(src_path, "Invalid header")
            is_return = True
            return_val = int(header[2])
        elif header[1] == "error":
            is_error = True
        elif header[1] == "exception":
            is_except = True
        else:
            return fail(src_path, "Invalid header")

        timeout_remain = TIMEOUT

        start = time.clock()
        if llvm:
            outname = TEMP_DIR + src_name + "." + LLVM_SUFFIX
            temp_files.append(outname)
            try:
                if verbose and not is_error:
                    retval = subprocess.call(
                        compiler_opts + ["-I", runtime_dir] +
                        ["-o", outname, src_path],
                        timeout=timeout_remain)
                else:
                    retval = subprocess.call(
                        compiler_opts + ["-I", runtime_dir] +
                        ["-o", outname, src_path],
                        timeout=timeout_remain, stdout=DEV_NULL,
                        stderr=subprocess.STDOUT)
            except subprocess.TimeoutExpired:
                return fail(src_path, "Compile timed out")
        else:
            outname = TEMP_DIR + src_name
            temp_files.append(outname)
            try:
                if verbose and not is_error:
                    retval = subprocess.call(
                        compiler_opts + ["-I", runtime_dir] +
                        ["-o", outname, src_path, runtime],
                        timeout=timeout_remain)
                else:
                    retval = subprocess.call(
                        compiler_opts + ["-I", runtime_dir] +
                        ["-o", outname, src_path, runtime],
                        timeout=timeout_remain, stdout=DEV_NULL,
                        stderr=subprocess.STDOUT)
            except subprocess.TimeoutExpired:
                return fail(src_path, "Compile timed out")

        end = time.clock()


        timeout_remain -= end - start

        if is_error:
            if retval != 0:
                return success(src_path)
            elif not llvm: #llvm may fail in linkage phase
                return fail(src_path, "Compilation unexpectedly suceeded")

        if retval != 0:
            return fail(src_path, "failed to compile")

        if llvm:
            exec_name = TEMP_DIR + src_name
            temp_files.append(exec_name)
            if verbose:
                retval = subprocess.call([CLANG, outname, runtime, "-o",
                                          exec_name])
            else:
                retval = subprocess.call([CLANG, outname, runtime, "-o",
                                          exec_name], stdout=DEV_NULL,
                                         stderr=subprocess.STDOUT)
            if retval != 0:
                if is_error:
                    return success(src_path)
                else:
                    return fail(src_path, "failed to create valid llvm ir")
            elif is_error:
                return fail(src_path, "Compilation unexpectedly suceeded")

            outname = exec_name

        try:
            if verbose:
                lines = subprocess.check_output(outname, timeout=timeout_remain,
                                                universal_newlines=True)
            else:
                lines = subprocess.check_output(outname, timeout=timeout_remain,
                                                universal_newlines=True,
                                                stderr=DEV_NULL)
        except subprocess.TimeoutExpired:
            if is_except:
                return success(src_path)
            else:
                return fail(src_path, "Executable timed out")

        except subprocess.CalledProcessError:
            if is_except:
                return success(src_path)
            else:
                return fail(src_path, "Unexpected exception")

        if is_except:
            return fail(src_path, "Expected exception")

        # Return value is last line of output
        lines = lines.split("\n")
        assert(len(lines) > 1)

        # Use -2 because there will be an empty string at end
        retval = int(lines[len(lines) - 2]);

        if is_return:
            if return_val != retval:
                return fail(src_path,
                            "Returned %d expected %d" % (retval, return_val))
            else:
                return success(src_path)

        # Shouldn't get here
        assert(False)

    finally:
        for filename in temp_files:
            try:
                os.remove(filename)
            except OSError:
                pass

# Run main
main()
