#!/usr/bin/env python3
"""Verify standby rotation and usage support on hardware through opt-in +LOG diagnostics."""
import argparse
from datetime import datetime
import re
import secrets
import time

import serial
from test_at_serial import expect_ok, expect_error, query_value
from test_ui_serial import hello, synchronize


def diagnostic(port):
    # INFO diagnostics can extend past the command response's first read chunk.
    # Read whole lines here so a partial +LOG tail is never discarded after OK.
    port.reset_input_buffer()
    port.write(b'AT+INFO?\r')
    port.flush()
    lines = []
    pending = bytearray()
    row = None
    ok = False
    deadline = time.monotonic() + 1.5
    while time.monotonic() < deadline:
        pending.extend(port.readline())
        if not pending.endswith(b'\n'):
            continue
        line = pending.decode('ascii', errors='replace').strip()
        pending.clear()
        lines.append(line)
        if line == 'OK':
            ok = True
        if line.startswith('+LOG:') and 'usage_supported=' in line:
            row = dict((key, int(value)) for key, value in re.findall(r'(\w+)=(\d+)', line))
        if ok and row is not None:
            return row
    raise AssertionError(lines)


def wait_page(port, page, supported, timeout=1):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        row = diagnostic(port)
        assert row['usage_supported'] == supported, row
        if row['page'] == page:
            return row
        time.sleep(.1)
    raise AssertionError(f'Expected page {page}, last diagnostics {row}')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('port')
    args = parser.parse_args()
    with serial.Serial(args.port, 115200, timeout=.1, write_timeout=2) as port:
        port.dtr = False
        port.rts = False
        time.sleep(2.2)
        port.reset_input_buffer()
        assert query_value(port, 'AT+MODE?', '+MODE:') == 'NORMAL'
        logs = query_value(port, 'AT+LOG?', '+LOG:')
        try:
            hello(port)
            expect_ok(port, 'AT+LOG=1')
            epoch = secrets.token_hex(16)
            synchronize(port, epoch)
            offset = int(datetime.now().astimezone().utcoffset().total_seconds() / 60)
            expect_ok(port, f'AT+TIME={int(time.time()*1000)},{offset}')
            wait_page(port, 4, 0)
            def usage(batch):
                expect_ok(port, f'AT+USAGEBEGIN={epoch},codex,{batch}')
                expect_ok(port, f'AT+USAGE={epoch},codex,l1,primary,-,0,18000,{int(time.time())},-')
            usage(1)
            wait_page(port, 4, 0)  # Pending data cannot enable quota display.
            expect_error(port, f'AT+USAGE={epoch},codex,l1,primary,10001,0,18000,0,-', 'INVALID_ARG')
            expect_error(port, f'AT+USAGEEND={epoch},codex,1', 'BAD_STATE')
            wait_page(port, 4, 0)
            usage(2)
            expect_ok(port, f'AT+USAGEEND={epoch},codex,2')
            first = wait_page(port, 3, 1)
            start = time.monotonic()
            previous = first['page']
            transitions = []
            intervals = []
            last_sample = 0
            next_update = 1
            batch = 3
            while time.monotonic() - start < 22:
                elapsed = time.monotonic() - start
                if elapsed >= next_update:
                    usage(batch)
                    expect_ok(port, f'AT+USAGEEND={epoch},codex,{batch}')
                    batch += 1
                    next_update += 1
                row = diagnostic(port)
                assert row['usage_supported'] == 1 and row['page'] in (3, 4), row
                sample = time.monotonic() - start
                if row['page'] != previous:
                    transitions.append((sample, row['page']))
                    intervals.append((last_sample, sample))
                    previous = row['page']
                last_sample = sample
                time.sleep(.15)
            assert len(transitions) == 2 and [page for _, page in transitions] == [4, 3], transitions
            # Continuous batches delay queries; the boundary must lie between observed samples.
            # Allow initial query latency and the 50 ms UI tick, rather than asserting its receive time.
            for boundary, (before, after) in zip((10, 20), intervals):
                assert before - .25 <= boundary <= after + .25, (boundary, intervals)
            print(f'PASS standby transitions despite continuous updates: {transitions}; sample intervals: {intervals}')
            def session(rev, state):
                result = 'completed' if state == 'task_success' else '-'
                finished = int(time.time()) if result != '-' else 0
                expect_ok(port, f'AT+SESSION={epoch},s1,{rev},codex,{state},{result},{finished},10,,,,,idle_test')
            session(1, 'permission_required')
            wait_page(port, 1, 1)
            session(2, 'task_success')
            wait_page(port, 2, 1)
            session(3, 'ready')
            wait_page(port, 3, 1)
            expect_ok(port, f'AT+DELETE={epoch},s1')
            expect_ok(port, f'AT+USAGECLEAR={epoch},codex')
            wait_page(port, 4, 1, 11)
            wait_page(port, 3, 1, 11)
            hello(port)
            synchronize(port, secrets.token_hex(16))
            wait_page(port, 4, 0)
            expect_error(port, f'AT+USAGEBEGIN={epoch},codex,1', 'BAD_STATE')
            epoch = secrets.token_hex(16)
            synchronize(port, epoch)
            usage(1)
            expect_ok(port, f'AT+USAGEEND={epoch},codex,1')
            wait_page(port, 3, 1)
            synchronize(port, secrets.token_hex(16))
            wait_page(port, 4, 0)
            print('Standby serial checks passed: 10-second rotation, task/result priority, clear and connection reset.')
        finally:
            hello(port)
            synchronize(port, secrets.token_hex(16))
            expect_ok(port, 'AT+LOG=' + logs)


if __name__ == '__main__':
    main()
