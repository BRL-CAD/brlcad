# BRL-CAD
#
# Copyright (c) 2026 United States Government as represented by
# the U.S. Army Research Laboratory.
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public License
# version 2.1 as published by the Free Software Foundation.
#
# This library is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this file; see the file named COPYING for more
# information.
#
#

import fcntl
import os
import pty
import select
import struct
import subprocess
import sys
import termios
import time


def read_until(master, token):
    data = b""
    deadline = time.monotonic() + 5
    while token not in data and time.monotonic() < deadline:
        if select.select([master], [], [], 0.1)[0]:
            data += os.read(master, 65536)
    if token not in data:
        raise RuntimeError(f"Missing {token!r} in {data!r}")
    return data


def drain(master):
    data = b""
    while select.select([master], [], [], 0.1)[0]:
        data += os.read(master, 65536)
    return data


def resize(slave, columns):
    fcntl.ioctl(slave, termios.TIOCSWINSZ, struct.pack("HHHH", 24, columns, 0, 0))


def check_resize(highlight):
    master, slave = pty.openpty()
    resize(slave, 80)
    process = subprocess.Popen(
        [sys.argv[1]] + (["highlight"] if highlight else []),
        stdin=slave, stdout=slave, stderr=slave,
        env={**os.environ, "TERM": "xterm"})
    try:
        read_until(master, b"test> ")
        command = ("alpha_café_" + "x" * 140 + "_omega").encode()
        os.write(master, command)
        display = drain(master)
        if highlight and b"\x1b[36m" not in display:
            raise RuntimeError("Highlighted input did not use the valid-token style")
        # No keystrokes accompany these resizes: the idle editor must repaint.
        for columns in (20, 2, 120):
            resize(slave, columns)
            read_until(master, b"\x1b[0K")
        os.write(master, b"\r")
        read_until(master, b"RESULT:" + command)
        if process.wait(timeout=5) != 0:
            raise RuntimeError("Line editor exited unsuccessfully")
    finally:
        if process.poll() is None:
            process.terminate()
            process.wait(timeout=5)
        os.close(master)
        os.close(slave)


if __name__ == "__main__":
    for highlight in (False, True):
        check_resize(highlight)
