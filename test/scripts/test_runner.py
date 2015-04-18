#!/usr/bin/env python3

import os
import sys
import subprocess
import time

HEADER_STR = "//test"
TEMP_NAME = "temp"
LLVM_SUFFIX = "ll"
TIMEOUT = 5
CC = "cc"
LLC = "llc"

DEFAULT_VERBOSITY = True

DEV_NULL = open(os.devnull, 'w')

def usage():
    print("usage: %s [-llvm] [test] [compiler and options]\n" % sys.argv[0])
    print("test must be a c source file with a correct first line header:")
    print("%s [return n | error | except | noreturn ]" % HEADER_STR)

    print("\noptional arguments:");
    print("-llvm Compiles to llvm ir")
    sys.exit(-1)

def fail(src_name, msg):
    print("FAIL: " + src_name + " " + msg)
    sys.exit(-1)

def success(src_name, verbose):
    if verbose:
        print("PASS: " + src_name)
    sys.exit(0)

def main():
    if len(sys.argv) < 3:
        usage()

    verbose = DEFAULT_VERBOSITY
    llvm = False

    if (sys.argv[1] == "-llvm"):
        llvm = True
        args = sys.argv[2:]
    else:
        args = sys.argv[1:]

    src_name = args[0]
    src = open(src_name)
    header = src.readline()
    src.close()

    header = header[:len(header) - 1] # Remove trailing newline
    header = header.split(" ")

    if len(header) < 2 or header[0] != HEADER_STR:
        usage()

    is_return = False
    return_val = 0
    is_error = False
    is_except = False
    is_noreturn = False

    if header[1] == "return":
        if len(header) < 3:
            usage()
        is_return = True
        return_val = int(header[2])
    elif header[1] == "error":
        is_error = True
    elif header[1] == "except":
        is_except = True
    elif header[1] == "noreturn":
        is_noreturn = True

    timeout_remain = TIMEOUT

    if llvm:
        outname = TEMP_NAME + "." + LLVM_SUFFIX
    else:
        outname = TEMP_NAME

    start = time.clock()
    try:
        if verbose:
            retval = subprocess.call(
                args[1:] + ["-o", "./" + outname] + [src_name],
                timeout=timeout_remain)
        else:
            retval = subprocess.call(
                args[1:] + ["-o", "./" + outname] + [src_name],
                timeout=timeout_remain, stdout=DEV_NULL,
                stderr=subprocess.STDOUT)
    except subprocess.TimeoutExpired:
        fail(src_name, "Compile timed out")

    end = time.clock()
    timeout_remain -= end - start

    if llvm:
        asm_name = TEMP_NAME + "." + "S"
        retval = subprocess.call([LLC, outname, "-o", asm_name])
        if retval != 0:
            fail(src_name, "failed to llvm ir")

        retval = subprocess.call([CC, asm_name, "-o", TEMP_NAME])
        assert(retval == 0) #LLC better produce valid output...

    if is_error:
        if retval == 0:
            fail(src_name, "Compilation unexpectedly suceeded")
        else:
            success(src_name, verbose)

    if retval != 0:
        fail(src_name, "failed to compile")

    try:
        retval = subprocess.call("./" + TEMP_NAME, timeout=timeout_remain)
    except subprocess.TimeoutExpired:
        if is_noreturn:
            success(src_name, verbose)
        else:
            fail(src_name, "Executable timed out")

    if is_except:
        if os.WIFSIGNALED(retval):
            success(src_name, verbose)
        else:
            fail(src_name, "Expected exception")

    if is_return:
        if return_val != retval:
            fail(src_name, "Returned %d expected %d" % retval, return_val)
        else:
            success(src_name, verbose)

# Run main
main()
