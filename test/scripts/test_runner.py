#!/usr/bin/env python3

import os
import sys
import subprocess
import time
import multiprocessing

RUNTIME_DIR = "./test/runtime/"
RUNTIME = os.path.join(RUNTIME_DIR, "runtime.c")
HEADER_STR = "//test"
TEMP_DIR = "/tmp/ccc/"
LLVM_SUFFIX = "ll"
TIMEOUT = 5
CC = "cc"
CLANG = "clang"

DEV_NULL = open(os.devnull, 'w')

NUM_PROCS = 4

verbose = False
llvm = False
compiler_opts = None

def usage():
    print("usage: %s [-llvm] [-v] [-jN] [compiler and options] [tests] \n"
          % sys.argv[0])
    print("test must be a c source file with a correct first line header:")
    print("%s [return n | error | exception ]" % HEADER_STR)

    print("\noptional arguments:");
    print("-j Jobs to run in parallel")
    print("-v Verbose - Prints passed tests and compiler's stdout/stderr")
    print("-llvm Compiles to llvm ir")
    sys.exit(-1)

def fail(src_path, msg):
    print("FAIL: " + src_path + " " + msg)

def success(src_path):
    if verbose:
        print("PASS: " + src_path)

def main():
    global verbose
    global llvm
    global compiler_opts

    if len(sys.argv) < 3:
        usage()

    args = sys.argv[1:]
    jobs = 1

    while args[0][0] == "-":
        if args[0] == "-llvm":
            llvm = True
        elif args[0] == "-v":
            verbose = True
        elif args[0][:2] == "-j":
            jobs = int(args[0][2:])
        else:
            usage()
        args = args[1:]

    compiler_opts = args[0].split()
    src_files = args[1:]

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

    try:
        src = open(src_path)
        header = src.readline()
        src.close()
        src_name = os.path.splitext(os.path.basename(src_path))[0]

        header = header[:len(header) - 1] # Remove trailing newline
        header = header.split(" ")

        if len(header) < 2 or header[0] != HEADER_STR:
            fail(src_path, "Invalid header")
            return 0

        is_return = False
        return_val = 0
        is_error = False
        is_except = False

        if header[1] == "return":
            if len(header) < 3:
                fail(src_path, "Invalid header")
                return 0
            is_return = True
            return_val = int(header[2])
        elif header[1] == "error":
            is_error = True
        elif header[1] == "exception":
            is_except = True
        else:
            fail(src_path, "Invalid header")
            return 0

        timeout_remain = TIMEOUT

        start = time.clock()
        if llvm:
            outname = TEMP_DIR + src_name + "." + LLVM_SUFFIX
            temp_files.append(outname)
            try:
                if verbose and not is_error:
                    retval = subprocess.call(
                        compiler_opts + ["-I", RUNTIME_DIR] +
                        ["-o", outname, src_path],
                        timeout=timeout_remain)
                else:
                    retval = subprocess.call(
                        compiler_opts + ["-I", RUNTIME_DIR] +
                        ["-o", outname, src_path],
                        timeout=timeout_remain, stdout=DEV_NULL,
                        stderr=subprocess.STDOUT)
            except subprocess.TimeoutExpired:
                fail(src_path, "Compile timed out")
                return 0
        else:
            outname = TEMP_DIR + src_name
            temp_files.append(outname)
            try:
                if verbose and not is_error:
                    retval = subprocess.call(
                        compiler_opts + ["-I", RUNTIME_DIR] +
                        ["-o", outname, src_path, RUNTIME],
                        timeout=timeout_remain)
                else:
                    retval = subprocess.call(
                        compiler_opts + ["-I", RUNTIME_DIR] +
                        ["-o", outname, src_path, RUNTIME],
                        timeout=timeout_remain, stdout=DEV_NULL,
                        stderr=subprocess.STDOUT)
            except subprocess.TimeoutExpired:
                fail(src_path, "Compile timed out")
                return 0

        end = time.clock()


        timeout_remain -= end - start

        if is_error:
            if retval == 0 and not llvm:
                fail(src_path, "Compilation unexpectedly suceeded")
                return 0
            else:
                success(src_path)
                return 1

        if retval != 0:
            fail(src_path, "failed to compile")
            return 0

        if llvm:
            exec_name = TEMP_DIR + src_name
            temp_files.append(exec_name)
            if verbose:
                retval = subprocess.call([CLANG, outname, RUNTIME, "-o",
                                          exec_name])
            else:
                retval = subprocess.call([CLANG, outname, RUNTIME, "-o",
                                          exec_name], stdout=DEV_NULL,
                                         stderr=subprocess.STDOUT)
            if retval != 0:
                if is_error:
                    return 1
                else:
                    fail(src_path, "failed to create valid llvm ir")
                    return 0
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
                success(src_path)
                return 1
            else:
                fail(src_path, "Executable timed out")
                return 0

        except subprocess.CalledProcessError:
            if is_except:
                success(src_path)
                return 1
            else:
                fail(src_path, "Unexpected exception")
                return 0

        if is_except:
            fail(src_path, "Expected exception")
            return 0

        # Return value is last line of output
        lines = lines.split("\n")
        assert(len(lines) > 1)

        # Use -2 because there will be an empty string at end
        retval = int(lines[len(lines) - 2]);

        if is_return:
            if return_val != retval:
                fail(src_path, "Returned %d expected %d" % (retval, return_val))
                return 0
            else:
                success(src_path)
                return 1

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
