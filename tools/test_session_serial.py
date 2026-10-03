#!/usr/bin/env python3
"""Verify highest-group rotation and submitted LED frames on the connected device."""
import argparse
import secrets
import time
import serial
from test_at_serial import expect_ok, query_value
from test_ui_serial import hello, synchronize
from test_idle_serial import diagnostic


def wait(port, predicate, timeout=1):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        row = diagnostic(port)
        if predicate(row):
            return row
        time.sleep(.05)
    raise AssertionError(row)


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
            expect_ok(port, 'AT+LOG=1')
            hello(port)
            wait(port, lambda r: r['led'] == 2 and r['rgb'] == 0xff9f1c and r['mask'].bit_count() == 8)
            epoch = secrets.token_hex(16)
            synchronize(port, epoch)
            wait(port, lambda r: r['page'] == 4 and r['led'] == 0 and r['mask'] == 0)
            def session(key, state, rev=1):
                result = {'task_success':'completed','task_failed':'failed','task_canceled':'cancelled'}.get(state, '-')
                finished = int(time.time()) if result != '-' else 0
                expect_ok(port, f'AT+SESSION={epoch},{key},{rev},codex,{state},{result},{finished},10,,,,,led_test')
            # Stable slots: work=1, thinking=2, stale=3, success=4, failed=5, canceled=6, input=7, permission=8.
            session('w', 'working')
            session('t', 'thinking')
            session('u', 'stale')
            session('s', 'task_success')
            wait(port, lambda r: r['group'] == 2 and r['slot'] == 4 and r['led'] == 7 and r['rgb'] == 0x36e29a)
            session('f', 'task_failed')
            session('c', 'task_canceled')
            session('i', 'waiting_for_input')
            first = wait(port, lambda r: r['group'] == 1 and r['slot'] == 7 and r['rgb'] == 0xffb02e and r['led'] == 8)
            start = time.monotonic()
            time.sleep(2)
            session('p', 'permission_required')
            row = diagnostic(port)
            assert row['slot'] == 7 and row['page_seq'] == first['page_seq'], row
            # EVENT and revision updates must not interrupt or restart dwell.
            expect_ok(port, f'AT+EVENT={epoch},{secrets.token_hex(16)},p,codex,permission_required,{int(time.time())}')
            session('i', 'waiting_for_input', 2)
            assert diagnostic(port)['slot'] == 7
            row = wait(port, lambda r: r['slot'] == 8, 9)
            elapsed = time.monotonic() - start
            assert abs(elapsed - 10) < .5, elapsed
            assert row['rgb'] == 0xff5f7a and row['led'] == 3, row
            print(f'PASS attention joins without interruption; rotation at {elapsed:.2f}s', flush=True)
            # INFO keeps entry effect while background scheduler rotates to input.
            expect_ok(port, 'AT+SHOWINFO=15')
            wait(port, lambda r: r['page'] == 5 and r['rgb'] == 0xff5f7a)
            until = time.monotonic() + 10.4
            while time.monotonic() < until:
                row = diagnostic(port)
                assert row['page'] == 5 and row['rgb'] == 0xff5f7a, row
                time.sleep(.2)
            expect_ok(port, 'AT+SHOWINFO=0')
            wait(port, lambda r: r['slot'] == 7 and r['rgb'] == 0xffb02e and r['led'] == 8)
            expect_ok(port, f'AT+DELETE={epoch},i')
            wait(port, lambda r: r['slot'] == 8)
            expect_ok(port, f'AT+DELETE={epoch},p')
            wait(port, lambda r: r['slot'] == 4 and r['group'] == 2 and r['rgb'] == 0x36e29a)
            row = wait(port, lambda r: r['slot'] == 5, 11)
            assert row['rgb'] == 0xff3b30 and row['led'] == 9, row
            row = wait(port, lambda r: r['slot'] == 6, 11)
            assert row['rgb'] == 0x4aa8ff and row['led'] == 10, row
            print('PASS result group excludes work; green/red/blue follow 10-second rotation', flush=True)
            for key in ('s', 'f', 'c'):
                expect_ok(port, f'AT+DELETE={epoch},{key}')
            row = wait(port, lambda r: r['slot'] == 1 and r['group'] == 3)
            assert row['led'] == 4 and row['mask'].bit_count() == 6 and row['rgb'] == 0x36e29a, row
            masks = set()
            until = time.monotonic() + 1.5
            while time.monotonic() < until:
                row = diagnostic(port)
                assert row['slot'] == 1 and row['mask'].bit_count() == 6 and row['led_sum'] == 1607, row
                masks.add(row['mask'])
                time.sleep(.08)
            assert len(masks) > 3, masks
            row = wait(port, lambda r: r['slot'] == 2, 10)
            assert row['led'] == 5 and row['mask'].bit_count() == 6 and row['rgb'] == 0x8b99ff, row
            row = wait(port, lambda r: r['slot'] == 3, 11)
            assert row['led'] == 6 and row['rgb'] == 0xa3afd3, row
            pixels = set()
            until = time.monotonic() + 3.2
            while time.monotonic() < until:
                row = diagnostic(port)
                assert row['mask'] == 0x0707 and row['led_sum'] == 1236, row
                pixels.add((row['p0'], row['p8']))
                time.sleep(.1)
            assert len(pixels) == 2, pixels
            print('PASS work/thinking/stale rotate with continuous six-light tails and fixed alternating arcs', flush=True)
            for key in ('w', 't', 'u'):
                expect_ok(port, f'AT+DELETE={epoch},{key}')
            wait(port, lambda r: r['page'] == 4 and r['led'] == 0 and r['mask'] == 0)
            # A new HELLO overrides INFO, followed by the 30-second disconnect watchdog.
            expect_ok(port, 'AT+SHOWINFO=60')
            hello(port)
            wait(port, lambda r: r['led'] == 2 and r['mask'].bit_count() == 8)
            time.sleep(30.2)
            row = wait(port, lambda r: r['led'] == 1 and r['rgb'] == 0xff3b30 and r['mask'] == 0x0707 and r['led_sum'] == 1302, 4.5)
            print('PASS idle LEDs off and disconnect overrides INFO; diagnostics:', row, flush=True)
        finally:
            hello(port)
            synchronize(port, secrets.token_hex(16))
            expect_ok(port, 'AT+LOG=' + logs)


if __name__ == '__main__':
    main()
