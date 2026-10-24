#!/usr/bin/env python3
"""Verify standby theme commands and persistence, restoring the original setting.

Run with the host service disconnected. --reboot temporarily enters factory mode;
--timeout also waits for the link timeout. Both restore NORMAL and the theme.
"""
import argparse
import secrets
import time

import serial
from test_at_serial import expect_error, expect_ok, query_value
from test_idle_serial import diagnostic
from test_session_serial import wait


def current(port):
    return query_value(port, 'AT+CLOCKTHEME?', '+CLOCKTHEME:')


def verify_preview(port, epoch):
    rev = 0
    def session(state):
        nonlocal rev
        rev += 1
        expect_ok(port, f'AT+SESSION={epoch},preview,{rev},codex,{state},-,0,10,,,,,clock_preview,LOCAL,S1')

    session('working')
    wait(port, lambda r: r['page'] == 1 and r['group'] == 3)
    started = time.monotonic()
    expect_ok(port, 'AT+CLOCKRGB=18,171,239')
    wait(port, lambda r: r['page'] == 4, .8)
    while time.monotonic() - started < 4.6:
        assert current(port) == 'CUSTOM'  # Queries must not extend the deadline.
        row = diagnostic(port)
        assert row['page'] == 4 and row['led'] == 4, row
        time.sleep(.1)
    wait(port, lambda r: r['page'] == 1, 1)
    elapsed = time.monotonic() - started
    assert 4.8 <= elapsed <= 5.5, elapsed
    print(f'PASS clock preview lasts {elapsed:.2f}s and preserves working LED', flush=True)

    expect_ok(port, 'AT+CLOCKTHEME=MINT')
    wait(port, lambda r: r['page'] == 4, .8)
    time.sleep(2)
    renewed = time.monotonic()
    expect_ok(port, 'AT+CLOCKTHEME=MINT')
    while time.monotonic() - renewed < 4.6:
        assert diagnostic(port)['page'] == 4
        time.sleep(.1)
    wait(port, lambda r: r['page'] == 1, 1)
    assert time.monotonic() - renewed >= 4.8
    print('PASS identical theme setting renews the five-second preview', flush=True)

    for urgent in ('permission_required', 'waiting_for_input'):
        expect_ok(port, 'AT+CLOCKTHEME=WHITE')
        wait(port, lambda r: r['page'] == 4, .8)
        session(urgent)
        wait(port, lambda r: r['page'] == 1 and r['group'] == 1, .8)
        expect_ok(port, 'AT+CLOCKRGB=18,171,239')
        time.sleep(.15)
        row = diagnostic(port)
        assert row['page'] == 1 and row['group'] == 1, row
        assert current(port) == 'CUSTOM'
        session('working')
        wait(port, lambda r: r['page'] == 1 and r['group'] == 3, .8)
        time.sleep(.15)
        assert diagnostic(port)['page'] == 1  # Cancelled preview must not resume.
        print('PASS urgent session blocks/cancels preview:', urgent, flush=True)
    expect_ok(port, f'AT+DELETE={epoch},preview')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('port')
    parser.add_argument('--reboot', action='store_true')
    parser.add_argument('--timeout', action='store_true')
    parser.add_argument('--preview', action='store_true', help='Verify five-second previews and urgent-session protection')
    args = parser.parse_args()
    port = serial.Serial(port=None, baudrate=115200, timeout=.1, write_timeout=2)
    port.dtr = False
    port.rts = False
    port.port = args.port
    with port:
        time.sleep(2.2)
        port.reset_input_buffer()
        assert query_value(port, 'AT+MODE?', '+MODE:') == 'NORMAL'
        original = current(port)
        original_rgb = query_value(port, 'AT+CLOCKRGB?', '+CLOCKRGB:')
        original_logs = query_value(port, 'AT+LOG?', '+LOG:')
        try:
            for rgb in ('0,0,0', '255,255,255', '0,255,128', '18,171,239'):
                expect_ok(port, 'AT+CLOCKRGB=' + rgb)
                assert query_value(port, 'AT+CLOCKRGB?', '+CLOCKRGB:') == rgb
                assert current(port) == 'CUSTOM'
                expect_ok(port, 'AT+CLOCKRGB=' + rgb)
            for suffix in ('', '=', '=1,2', '=1,2,3,4', '=-1,0,0', '=256,0,0', '=1,2,3 ', '= 1,2,3', '=1,,3', '=1.5,2,3'):
                expect_error(port, 'AT+CLOCKRGB' + suffix, 'INVALID_ARG')
                assert query_value(port, 'AT+CLOCKRGB?', '+CLOCKRGB:') == '18,171,239'
                assert current(port) == 'CUSTOM'
            for theme in ('AMBER', 'WHITE', 'MINT', 'CUSTOM', 'CODEX'):
                expect_ok(port, 'AT+CLOCKTHEME=' + theme)
                assert current(port) == theme
                expect_ok(port, 'AT+CLOCKTHEME=' + theme)
                assert current(port) == theme
            for suffix in ('', '=', '=PURPLE', '=amber', '=0', '=MINT,WHITE', '=WHITE ', '= AMBER'):
                expect_error(port, 'AT+CLOCKTHEME' + suffix, 'INVALID_ARG')
                assert current(port) == 'CODEX'
            expect_ok(port, 'AT+HELLO=2,' + secrets.token_hex(16))
            epoch = secrets.token_hex(16)
            expect_ok(port, f'AT+SYNCBEGIN={epoch},1')
            expect_ok(port, f'AT+SYNCEND={epoch},1')
            expect_ok(port, f'AT+TIME={int(time.time()*1000)},480')
            assert current(port) == 'CODEX'
            if args.preview:
                expect_ok(port, 'AT+LOG=1')
                verify_preview(port, epoch)
            if args.timeout:
                time.sleep(31)
                assert current(port) == 'CODEX'
            if args.reboot:
                expect_ok(port, 'AT+CLOCKTHEME=CUSTOM')
                expect_ok(port, 'AT+MODE=FACTORY')
                time.sleep(2.5)
                assert query_value(port, 'AT+MODE?', '+MODE:') == 'FACTORY'
                expect_error(port, 'AT+CLOCKTHEME?', 'UNSUPPORTED_FEATURE')
                expect_error(port, 'AT+CLOCKTHEME=AMBER', 'UNSUPPORTED_FEATURE')
                expect_error(port, 'AT+CLOCKRGB?', 'UNSUPPORTED_FEATURE')
                expect_error(port, 'AT+CLOCKRGB=1,2,3', 'UNSUPPORTED_FEATURE')
                expect_ok(port, 'AT+MODE=NORMAL')
                time.sleep(2.5)
                assert current(port) == 'CUSTOM'
                assert query_value(port, 'AT+CLOCKRGB?', '+CLOCKRGB:') == '18,171,239'
        finally:
            if query_value(port, 'AT+MODE?', '+MODE:') != 'NORMAL':
                expect_ok(port, 'AT+MODE=NORMAL')
                time.sleep(2.5)
            if args.preview:
                expect_ok(port, 'AT+HELLO=2,' + secrets.token_hex(16))
            expect_ok(port, 'AT+LOG=' + original_logs)
            expect_ok(port, 'AT+CLOCKRGB=' + original_rgb)
            expect_ok(port, 'AT+CLOCKTHEME=' + original)
            assert current(port) == original
        print('Clock theme validation passed; original theme and NORMAL mode restored')


if __name__ == '__main__':
    main()
