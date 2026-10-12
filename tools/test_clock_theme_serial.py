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


def current(port):
    return query_value(port, 'AT+CLOCKTHEME?', '+CLOCKTHEME:')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('port')
    parser.add_argument('--reboot', action='store_true')
    parser.add_argument('--timeout', action='store_true')
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
        try:
            for theme in ('AMBER', 'WHITE', 'MINT', 'CODEX'):
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
            if args.timeout:
                time.sleep(31)
                assert current(port) == 'CODEX'
            if args.reboot:
                expect_ok(port, 'AT+MODE=FACTORY')
                time.sleep(2.5)
                assert query_value(port, 'AT+MODE?', '+MODE:') == 'FACTORY'
                expect_error(port, 'AT+CLOCKTHEME?', 'UNSUPPORTED_FEATURE')
                expect_error(port, 'AT+CLOCKTHEME=AMBER', 'UNSUPPORTED_FEATURE')
                expect_ok(port, 'AT+MODE=NORMAL')
                time.sleep(2.5)
                assert current(port) == 'CODEX'
        finally:
            if query_value(port, 'AT+MODE?', '+MODE:') != 'NORMAL':
                expect_ok(port, 'AT+MODE=NORMAL')
                time.sleep(2.5)
            expect_ok(port, 'AT+CLOCKTHEME=' + original)
            assert current(port) == original
        print('Clock theme validation passed; original theme and NORMAL mode restored')


if __name__ == '__main__':
    main()
