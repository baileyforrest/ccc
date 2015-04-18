#!/usr/bin/env python3

import os
import sys
import subprocess
import time

RUNTIME = "./test/util/runtime.c"
HEADER_STR = "//test"
TEMP_NAME = "temp"
LLVM_SUFFIX = "ll"
TIMEOUT = 5
CC = "cc"
CLANG = "clang"

DEV_NULL = open(os.devnull, 'w')

failure = False

def usage():
    print("usage: %s [-llvm] [-v] [compiler and options] [tests] \n"
          % sys.argv[0])
    print("test must be a c source file with a correct first line header:")
    print("%s [return n | error | exception | noreturn ]" % HEADER_STR)

    print("\noptional arguments:");
    print("-v Verbose - Prints passed tests and compiler's stdout/stderr")
    print("-llvm Compiles to llvm ir")
    sys.exit(-1)

def fail(src_name, msg):
    global failure
    failure = True
    print("FAIL: " + src_name + " " + msg)

def success(src_name, verbose):
    if verbose:
        print("PASS: " + src_name)

def main():
    if len(sys.argv) < 3:
        usage()

    verbose = False
    llvm = False

    args = sys.argv[1:]

    while args[0][0] == "-":
        if args[0] == "-llvm":
            llvm = True
        elif args[0] == "-v":
            verbose = True
        else:
            usage()
        args = args[1:]

    compiler_opts = args[0].split()
    for src_name in args[1:]:
        src = open(src_name)
        header = src.readline()
        src.close()

        header = header[:len(header) - 1] # Remove trailing newline
        header = header.split(" ")

        if len(header) < 2 or header[0] != HEADER_STR:
            fail(src_name, "Invalid header")
            continue

        is_return = False
        return_val = 0
        is_error = False
        is_except = False
        is_noreturn = False

        if header[1] == "return":
            if len(header) < 3:
                fail(src_name, "Invalid header")
                continue
            is_return = True
            return_val = int(header[2])
        elif header[1] == "error":
            is_error = True
        elif header[1] == "exception":
            is_except = True
        elif header[1] == "noreturn":
            is_noreturn = True
        else:
            fail(src_name, "Invalid header")
            continue

        timeout_remain = TIMEOUT

        if llvm:
            outname = TEMP_NAME + "." + LLVM_SUFFIX
        else:
            outname = TEMP_NAME

        start = time.clock()
        try:
            if verbose and not is_error:
                retval = subprocess.call(
                    compiler_opts + ["-o", "./" + outname, src_name, RUNTIME],
                    timeout=timeout_remain)
            else:
                retval = subprocess.call(
                    compiler_opts + ["-o", "./" + outname, src_name, RUNTIME],
                    timeout=timeout_remain, stdout=DEV_NULL,
                    stderr=subprocess.STDOUT)
        except subprocess.TimeoutExpired:
            fail(src_name, "Compile timed out")
            continue

        end = time.clock()
        timeout_remain -= end - start

        if llvm:
            asm_name = TEMP_NAME + "." + "S"
            retval = subprocess.call([CLANG, outname, RUNTIME, "-o", asm_name])
            if retval != 0:
                fail(src_name, "failed to create valid llvm ir")
                continue

        if is_error:
            if retval == 0:
                fail(src_name, "Compilation unexpectedly suceeded")
            else:
                success(src_name, verbose)
            continue

        if retval != 0:
            fail(src_name, "failed to compile")
            continue

        try:
            lines = subprocess.check_output("./" + TEMP_NAME,
                                            timeout=timeout_remain,
                                            universal_newlines=True)
        except subprocess.TimeoutExpired:
            if is_noreturn:
                success(src_name, verbose)
            else:
                fail(src_name, "Executable timed out")
            continue

        except subprocess.CalledProcessError:
            if is_except:
                success(src_name, verbose)
            else:
                fail(src_name, "Unexpected exception")
            continue

        if is_except:
            fail(src_name, "Expected exception")
            continue

        # Return value is last line of output
        lines = lines.split("\n")
        assert(len(lines) > 1)

        # Use -2 because there will be an empty string at end
        retval = int(lines[len(lines) - 2]);

        if is_return:
            if return_val != retval:
                fail(src_name, "Returned %d expected %d" % (retval, return_val))
            else:
                success(src_name, verbose)
            continue

        # Shouldn't get here
        assert(False)

    if failure:
        sys.exit(1)

# Run main
main()
