#!/usr/bin/env python3
"""Verify active-session quota refreshes on hardware; host UI tests certify pixels.

Uses an isolated epoch and restores LOG/MODE. Stop the host service before running.
"""
import argparse
import secrets
import time

import serial
from test_at_serial import expect_ok, query_value
from test_idle_serial import diagnostic
from test_ui_serial import hello, synchronize


def wait_page(port, page):
    deadline = time.monotonic() + 2
    while time.monotonic() < deadline:
        row = diagnostic(port)
        if row['page'] == page:
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
        mode = query_value(port, 'AT+MODE?', '+MODE:')
        logs = query_value(port, 'AT+LOG?', '+LOG:')
        try:
            if mode != 'NORMAL':
                expect_ok(port, 'AT+MODE=NORMAL')
                time.sleep(2.5)
            hello(port)
            expect_ok(port, 'AT+LOG=1')
            epoch = secrets.token_hex(16)
            synchronize(port, epoch)
            expect_ok(port, f'AT+SESSION={epoch},s1,1,codex,working,-,0,10,,,,,quota_test,LOCAL,S1')
            first = wait_page(port, 1)
            sequence = first['page_seq']
            assert first['usage_supported'] == 0

            def check_refresh():
                time.sleep(.15)
                row = diagnostic(port)
                assert row['page'] == 1 and row['page_seq'] == sequence, row
                assert row['slot'] == 1 and row['status'] == first['status'], row
                assert row['render_max_us'] < 100000, row
                return row

            # Updates cover full, empty, thresholds, unknown and an omitted week window.
            for batch, used in enumerate((0, 4200, 5050, 5051, 8050, 8051, 10000, '-'), 1):
                expect_ok(port, f'AT+USAGEBEGIN={epoch},codex,{batch}')
                expect_ok(port, f'AT+USAGE={epoch},codex,l1,primary,{used},0,18000,{int(time.time())},-')
                pending = check_refresh()
                if batch == 1:
                    assert pending['usage_supported'] == 0
                if batch != 7:
                    expect_ok(port, f'AT+USAGE={epoch},codex,l1,secondary,9200,0,604800,{int(time.time())},-')
                expect_ok(port, f'AT+USAGEEND={epoch},codex,{batch}')
                assert check_refresh()['usage_supported'] == 1
            expect_ok(port, f'AT+USAGECLEAR={epoch},codex')
            assert check_refresh()['usage_supported'] == 1
            expect_ok(port, f'AT+SESSION={epoch},s1,2,claude,working,-,0,10,12000,200000,600,{int(time.time())},quota_test,LOCAL,S1')
            time.sleep(.15)
            assert diagnostic(port)['page'] == 1
            expect_ok(port, f'AT+SESSION={epoch},s1,3,codex,task_success,completed,{int(time.time())},10,,,,,quota_test,LOCAL,S1')
            row = wait_page(port, 2)
            print(f"PASS session quota batches, clear, provider/result transitions; render_max_us={row['render_max_us']}")
            expect_ok(port, f'AT+DELETE={epoch},s1')
        finally:
            expect_ok(port, 'AT+LOG=' + logs)
            if mode != 'NORMAL':
                expect_ok(port, 'AT+MODE=' + mode)


if __name__ == '__main__':
    main()
