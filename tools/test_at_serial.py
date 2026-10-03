#!/usr/bin/env python3
"""Exercise AgentMate AT interaction on the ESP32-C3 CH340 UART port."""

import argparse
import secrets
import time

import serial


def exchange(port, command, ending=b"\r", timeout=3.0):
    port.reset_input_buffer()
    port.write(command.encode("ascii") + ending)
    port.flush()
    deadline = time.monotonic() + timeout
    lines = []
    pending = bytearray()
    final = None
    while time.monotonic() < deadline:
        chunk = port.read(256)
        for byte in chunk:
            if byte == 10:
                line = pending.rstrip(b"\r").decode("ascii", errors="replace")
                pending.clear()
                if line:
                    lines.append(line)
                    if line in ("OK", "ERROR"):
                        final = line
            else:
                pending.append(byte)
        if final:
            break
    if final is None:
        raise AssertionError(f"{command}: timed out; received {lines!r}")
    return final, lines


def expect_ok(port, command, ending=b"\r"):
    final, lines = exchange(port, command, ending)
    if final != "OK":
        raise AssertionError(f"{command}: expected OK, received {lines!r}")
    print(f"PASS {command}")
    return lines


def expect_error(port, command, code):
    final, lines = exchange(port, command)
    if final != "ERROR" or f"+ERR:{code}" not in lines:
        raise AssertionError(f"{command}: expected {code}, received {lines!r}")
    print(f"PASS {command} -> {code}")


def query_value(port, command, prefix):
    lines = expect_ok(port, command)
    matches = [line[len(prefix):] for line in lines if line.startswith(prefix) and
               (prefix != "+LOG:" or "," not in line[len(prefix):])]
    if len(matches) != 1:
        raise AssertionError(f"{command}: expected one {prefix} line, received {lines!r}")
    return matches[0]


def test(port):
    mode = query_value(port, "AT+MODE?", "+MODE:")
    if mode not in ("FACTORY", "NORMAL"):
        raise AssertionError(f"unexpected mode: {mode!r}")
    nonce = secrets.token_hex(16)
    hello = query_value(port, f"AT+HELLO=1,{nonce}", "+HELLO:").split(",")
    if len(hello) != 9 or hello[:3] != ["1", "agentmate", nonce] or hello[-1] != mode:
        raise AssertionError(f"bad HELLO response: {hello!r}")
    print(f"Device {hello[3]}, mode {mode}")

    echo = query_value(port, "AT+ECHO?", "+ECHO:")
    logs = query_value(port, "AT+LOG?", "+LOG:")
    expected_default = "1" if mode == "FACTORY" else "0"
    if echo != expected_default or logs != expected_default:
        raise AssertionError(f"unexpected mode defaults: echo={echo}, log={logs}")

    info = expect_ok(port, "AT+INFZ\bO?")
    if not any(line.startswith("+INFO:") for line in info):
        raise AssertionError(f"backspace did not edit the command: {info!r}")
    expect_ok(port, "AT+MODE?", ending=b"\r\n")
    expect_ok(port, "AT+MODE?", ending=b"\n")
    expect_error(port, "AT+TESTLED=ALL,256,0,0,32" if mode == "FACTORY" else "AT+TESTAUTO=1",
                 "INVALID_ARG" if mode == "FACTORY" else "UNSUPPORTED_FEATURE")
    expect_error(port, "AT+" + "X" * 511, "LINE_TOO_LONG")
    expect_ok(port, "AT+INFO?")

    expect_ok(port, "AT+ECHO=0")
    if query_value(port, "AT+ECHO?", "+ECHO:") != "0":
        raise AssertionError("echo did not turn off")
    expect_ok(port, "AT+ECHO=1")
    if query_value(port, "AT+ECHO?", "+ECHO:") != "1":
        raise AssertionError("echo did not turn on")
    expect_ok(port, f"AT+ECHO={expected_default}")

    expect_ok(port, "AT+LOG=1")
    log_lines = expect_ok(port, "AT+INFO?")
    deadline = time.monotonic() + 1.0
    log_data = bytearray("\n".join(log_lines).encode("ascii"))
    while time.monotonic() < deadline:
        log_data.extend(port.read(256))
    if b"+LOG:" not in log_data:
        raise AssertionError("no structured application log after AT+INFO?")
    print("PASS structured +LOG output")
    expect_ok(port, f"AT+LOG={expected_default}")

    if mode == "FACTORY":
        commands = [
            "AT+TESTLED=0,255,0,0,32", "AT+TESTLED=ALL,0,0,255,32",
            "AT+TESTTFT=RED", "AT+TESTTFT=GREEN", "AT+TESTTFT=BLUE",
            "AT+TESTBACKLIGHT=64",
            *(f"AT+TESTFONT={name}" for name in ("8x8", "8x16", "10x18", "12x22", "16x32")),
            "AT+TESTAUTO=1", "AT+TESTAUTO=0",
        ]
        for command in commands:
            expect_ok(port, command)
            if command == "AT+TESTAUTO=1":
                time.sleep(8)
            elif command.startswith(("AT+TESTLED", "AT+TESTTFT", "AT+TESTBACKLIGHT", "AT+TESTFONT")):
                time.sleep(0.75)
        print("Inspect the TFT and all 16 LEDs visually; serial responses cannot prove pixel output.")
    else:
        expect_error(port, "AT+TESTTFT=RED", "UNSUPPORTED_FEATURE")
        ping_nonce = secrets.token_hex(4)
        pong = query_value(port, f"AT+PING={ping_nonce}", "+PONG:")
        if not pong.startswith(ping_nonce + ","):
            raise AssertionError(f"PING nonce mismatch: {pong!r}")
        query_value(port, "AT+TIME?", "+TIME:")
        query_value(port, "AT+LIST?", "+LIST:")
        for name in ("TFTBRIGHT", "LEDBRIGHT"):
            saved = int(query_value(port, f"AT+{name}?", f"+{name}:"))
            if not 0 <= saved <= 100:
                raise AssertionError(f"{name}: invalid brightness percent {saved}")
            try:
                for value in (0, 1, 25, 50, 75, 99, 100):
                    expect_ok(port, f"AT+{name}={value}")
                    actual = int(query_value(port, f"AT+{name}?", f"+{name}:"))
                    if actual != value:
                        raise AssertionError(f"{name}: {value}% round trip returned {actual}%")
                for value in (-1, 101, 255):
                    expect_error(port, f"AT+{name}={value}", "INVALID_ARG")
            finally:
                expect_ok(port, f"AT+{name}={saved}")
        query_value(port, "AT+TFTROTATE?", "+TFTROTATE:")
    print("AT interaction checks passed")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("port", help="ESP32-C3 CH340 UART0 port")
    args = parser.parse_args()
    with serial.Serial(args.port, 115200, timeout=0.1, write_timeout=2.0) as port:
        port.dtr = False
        port.rts = False
        time.sleep(1.5)
        test(port)


if __name__ == "__main__":
    main()
