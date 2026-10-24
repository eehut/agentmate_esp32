#!/usr/bin/env python3
"""Validate atomic Bluetooth battery snapshots and standby behavior on an explicit UART."""
import argparse
import secrets
import time
from urllib.parse import quote

import serial
from test_at_serial import expect_ok, expect_error, query_value
from test_session_serial import wait


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('port')
    args = parser.parse_args()
    port = serial.Serial(port=None, baudrate=115200, timeout=.1, write_timeout=2)
    port.dtr = False
    port.rts = False
    port.port = args.port
    with port:
        time.sleep(2.2)
        port.reset_input_buffer()
        assert query_value(port, 'AT+MODE?', '+MODE:') == 'NORMAL'
        logs = query_value(port, 'AT+LOG?', '+LOG:')
        epoch = secrets.token_hex(16)
        def current():
            return query_value(port, 'AT+BATTERY?', '+BATTERY:')
        def update(rows):
            fields = [epoch, str(len(rows))]
            for kind, value, connected, name in rows:
                fields += [kind, str(value), str(connected), quote(name, safe='')]
            expect_ok(port, 'AT+BATTERY=' + ','.join(fields))
        try:
            expect_ok(port, 'AT+LOG=1')
            expect_ok(port, 'AT+HELLO=2,' + secrets.token_hex(16))
            expect_ok(port, f'AT+SYNCBEGIN={epoch},1')
            expect_ok(port, f'AT+SYNCEND={epoch},1')
            assert current() == '4,0'
            rows = [('keyboard', 0, 1, 'MX Keys'), ('mouse', 30, 1, 'Mouse, A'),
                    ('keyboard', 60, 1, '键盘'), ('mouse', 100, 1, 'Magic Mouse')]
            for count in range(1, 5):
                update(rows[:count])
                assert current() == f'4,{count}'
            for argument in ('', '=', f'={epoch},5', f'={epoch},1,mouse,101,1,Mouse',
                             f'={epoch},1,mouse,20,2,Mouse', f'={epoch},1,audio,20,1,Mouse',
                             f'={epoch},1,mouse,20,1,%GG', f'={epoch},1,mouse,20,1,' + 'a' * 25,
                             f'={epoch},1,mouse,20,1,Mouse,extra'):
                expect_error(port, 'AT+BATTERY' + argument, 'INVALID_ARG')
                assert current() == '4,4'
            expect_error(port, 'AT+BATTERY=' + secrets.token_hex(16) + ',0', 'BAD_STATE')
            wait(port, lambda row: row['page'] == 7, 11)
            update([('keyboard', '-', 1, 'Unknown'), ('mouse', 80, 0, 'Disconnected')])
            wait(port, lambda row: row['page'] == 4, 1)
            update([('mouse', 60, 1, 'Mouse')])
            wait(port, lambda row: row['page'] == 7, 11)
            expect_ok(port, f'AT+SESSION={epoch},urgent,1,codex,permission_required,-,0,10,,,,,battery_test,LOCAL,S1')
            wait(port, lambda row: row['page'] == 1 and row['group'] == 1, 1)
            expect_ok(port, f'AT+DELETE={epoch},urgent')
            update([])
            wait(port, lambda row: row['page'] == 4, 1)
            assert current() == '4,0'
            print('Battery serial validation passed: counts, atomic errors, unknown/offline, standby rotation and attention priority')
        finally:
            expect_ok(port, 'AT+HELLO=2,' + secrets.token_hex(16))
            expect_ok(port, 'AT+LOG=' + logs)


if __name__ == '__main__':
    main()
