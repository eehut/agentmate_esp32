#!/usr/bin/env python3
"""Check submitted LED intensity, arc positions and complete cycles on hardware."""
import argparse
import secrets
import time

import serial
from test_at_serial import expect_ok, query_value
from test_idle_serial import diagnostic
from test_session_serial import wait
from test_ui_serial import hello, synchronize


def capture(port, mode, duration, sums, masks=None):
    wait(port, lambda r: r['led'] == mode)
    rows = []
    end = time.monotonic() + duration
    while time.monotonic() < end:
        row = diagnostic(port)
        assert row['led'] == mode and row['led_sum'] in sums, row
        if masks is not None:
            assert row['mask'] in masks, row
        rows.append(row)
        time.sleep(.04)
    assert {row['led_sum'] for row in rows} == sums, rows
    return rows


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('port')
    args = parser.parse_args()
    with serial.Serial(args.port, 115200, timeout=.1, write_timeout=2) as port:
        port.dtr = False
        port.rts = False
        time.sleep(2.2)
        port.reset_input_buffer()
        logs = query_value(port, 'AT+LOG?', '+LOG:')
        try:
            expect_ok(port, 'AT+LOG=1')
            hello(port)
            rows = capture(port, 2, 1.9, {2032})
            assert all(r['mask'].bit_count() == 8 for r in rows)
            assert len({r['mask'] for r in rows}) == 8
            print('PASS connecting: eight lights, fixed intensity and opposite rotating tails', flush=True)
            epoch = secrets.token_hex(16)
            synchronize(port, epoch)
            rows = capture(port, 0, .3, {0}, {0})
            cases = [
                ('working', 4, 2.8, {1607}, None),
                ('thinking', 5, 4.3, {1476}, None),
                ('stale', 6, 3.3, {1236}, {0x0707}),
                ('permission_required', 3, 2.3, {0, 4144}, {0, 0xffff}),
                ('waiting_for_input', 8, 2.3, {0, 2672}, {0, 0x0f0f}),
                ('task_success', 7, 2.7, {0, 956, 1912, 2868, 3824}, {0, 0x000f, 0x00ff, 0x0fff, 0xffff}),
                ('task_failed', 9, 2.7, {0, 3168}, {0, 0xffff}),
                ('task_canceled', 10, 3.3, {0, 1992}, {0, 0x0f0f}),
            ]
            for rev, (state, mode, duration, sums, masks) in enumerate(cases, 1):
                expect_ok(port, 'AT+PING=a1')
                result = {'task_success':'completed', 'task_failed':'failed', 'task_canceled':'cancelled'}.get(state, '-')
                finished = int(time.time()) if result != '-' else 0
                expect_ok(port, f'AT+SESSION={epoch},led,{rev},codex,{state},{result},{finished},10,,,,,led_v2,LOCAL,S1')
                rows = capture(port, mode, duration, sums, masks)
                if mode in (4, 5):
                    assert all(r['mask'].bit_count() == 6 for r in rows)
                    assert len({r['mask'] for r in rows}) >= (14 if mode == 4 else 8)
                if mode == 6:
                    assert len({(r['p0'], r['p8']) for r in rows}) == 2
                    assert all(r['p0'] != r['p8'] for r in rows)
                print(f'PASS {state}: {len(rows)} submitted frames, intensity stages {sorted(sums)}', flush=True)
            print('Final diagnostics:', rows[-1], flush=True)
        finally:
            hello(port)
            synchronize(port, secrets.token_hex(16))
            expect_ok(port, 'AT+LOG=' + logs)


if __name__ == '__main__':
    main()
