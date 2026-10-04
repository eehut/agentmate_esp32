#!/usr/bin/env python3
"""Validate phase-one UI AT protocol on hardware and optionally demonstrate pages.

Uses an isolated epoch, exercises rotation/NVS restoration, and optionally enters
FACTORY then restores the original mode. Does not certify physical pixel output.
"""
import argparse
from datetime import datetime
from pathlib import Path
import secrets
import time
from urllib.parse import quote

import serial
from test_at_serial import expect_error, expect_ok, query_value


def hello(port):
    return query_value(port, 'AT+HELLO=2,' + secrets.token_hex(16), '+HELLO:')


def synchronize(port, epoch, sync_id=1):
    expect_ok(port, f'AT+SYNCBEGIN={epoch},{sync_id}')
    expect_ok(port, f'AT+SYNCEND={epoch},{sync_id}')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('port')
    parser.add_argument('--demo', action='store_true', help='hold each page for visual inspection')
    parser.add_argument('--factory', action='store_true', help='test original factory commands and restore mode')
    parser.add_argument('--timeout', action='store_true', help='also wait 31 seconds to verify disconnect cleanup')
    args = parser.parse_args()
    with serial.Serial(args.port, 115200, timeout=.1, write_timeout=2) as port:
        # CH340 bridge: opening with asserted RTS can hold EN low. Release it.
        port.dtr = False
        port.rts = False
        time.sleep(2.2)
        port.reset_input_buffer()
        original_mode = query_value(port, 'AT+MODE?', '+MODE:')
        rotation = None
        try:
            if original_mode == 'FACTORY':
                expect_ok(port, 'AT+MODE=NORMAL')
                time.sleep(2.5)
            capacities = hello(port).split(',')
            max_sessions, max_usages = int(capacities[6]), int(capacities[7])
            assert (max_sessions, max_usages) == (8, 4), capacities
            expect_error(port, 'AT+TIME=1790936702000', 'INVALID_ARG')
            expect_error(port, 'AT+TIME=1790936702000,841', 'INVALID_ARG')
            for offset in (-840, -300, 0, 480, 840):
                expect_ok(port, f'AT+TIME={int(time.time()*1000)},{offset}')
                assert query_value(port, 'AT+TIME?', '+TIME:').split(',')[-1] == str(offset)
            expect_ok(port, 'AT+TIME=' + str(int(time.time()*1000)) + ',480')
            epoch = secrets.token_hex(16)
            synchronize(port, epoch)
            # TIME before initial SYNCBEGIN must remain valid.
            assert query_value(port, 'AT+TIME?', '+TIME:').split(',')[-1] == '480'
            encoded = quote('工程目录,agentmate', safe='-_.~')
            def session(key, rev, state, provider='claude', context='12000,200000,600,1790936702', duration=725, project=encoded):
                result = {'task_success':'completed', 'task_failed':'failed', 'task_canceled':'cancelled'}.get(state, '-')
                finished = int(time.time()) if result != '-' else 0
                if provider == 'codex':
                    context = ',,,'
                command = f'AT+SESSION={epoch},{key},{rev},{provider},{state},{result},{finished},{duration},{context},{project},LOCAL,S1'
                return expect_ok(port, command)
            session('s1', 1, 'working')
            row = query_value(port, 'AT+GET=s1', '+SESSION:').split(',')
            assert len(row) == 15 and row[12] == encoded and row[7] == '725' and row[13:] == ['LOCAL', 'S1']
            session('s1', 1, 'working')
            assert query_value(port, 'AT+GET=s1', '+SESSION:').split(',')[2] == '1'
            expect_error(port, f'AT+SESSION={epoch},bad,1,claude,working,-,0,10,1,100,100,1,LOCAL,S1', 'INVALID_ARG')
            expect_error(port, f'AT+SESSION={epoch},bad,1,codex,working,-,0,10,,,,,%GG,LOCAL,S1', 'INVALID_ARG')
            expect_error(port, f'AT+SESSION={epoch},bad,1,codex,working,-,0,10,,,,,%C0%AF,LOCAL,S1', 'INVALID_ARG')
            expect_error(port, f'AT+SESSION={epoch},bad,1,codex,working,-,0,10,,,,,' + 'a'*65 + ',LOCAL,S1', 'INVALID_ARG')
            for value in ('', '-1', '61', '10.5'):
                expect_error(port, f'AT+SHOWINFO={value}', 'INVALID_ARG')
            expect_ok(port, 'AT+SHOWINFO=10')
            remaining = int(query_value(port, 'AT+SHOWINFO?', '+SHOWINFO:')); assert 9 <= remaining <= 10
            session('s1', 2, 'permission_required')
            assert query_value(port, 'AT+SHOWINFO?', '+SHOWINFO:') == '0'
            expect_ok(port, 'AT+SHOWINFO=10')
            session('s1', 2, 'permission_required')  # retry cannot re-trigger attention
            assert int(query_value(port, 'AT+SHOWINFO?', '+SHOWINFO:')) > 0
            event = secrets.token_hex(16)
            command = f'AT+EVENT={epoch},{event},s1,claude,permission_required,{int(time.time())}'
            expect_ok(port, command)
            assert query_value(port, 'AT+SHOWINFO?', '+SHOWINFO:') == '0'
            expect_ok(port, 'AT+SHOWINFO=10'); expect_ok(port, command)
            assert int(query_value(port, 'AT+SHOWINFO?', '+SHOWINFO:')) > 0
            expect_ok(port, 'AT+SHOWINFO=1'); time.sleep(1.2)
            assert query_value(port, 'AT+SHOWINFO?', '+SHOWINFO:') == '0'
            expect_ok(port, 'AT+SHOWINFO=10'); expect_ok(port, 'AT+SHOWINFO=0')
            assert query_value(port, 'AT+SHOWINFO?', '+SHOWINFO:') == '0'
            for i, state in enumerate(('working','thinking','waiting_for_input','permission_required','stale',
                                        'task_success','task_failed','task_canceled','ready'), start=3):
                session('s1', i, state)
                if args.demo: time.sleep(2)
            session('s1', 12, 'working', context='?,?,?,?')
            session('s2', 1, 'working', provider='codex')
            session('s3', 1, 'task_failed')
            session('s4', 1, 'working', duration=4294967295, project='agentmate-provider-registry-dashboard')
            for slot in range(5, max_sessions + 1):
                session(f's{slot}', 1, 'working', provider='codex')
            session(f's{max_sessions}', 2, 'thinking', provider='codex')
            full_list = query_value(port, 'AT+LIST?', '+LIST:').split(',')
            assert full_list[1:] == [str(max_sessions), str(max_sessions)]
            expect_error(port, f'AT+SESSION={epoch},s{max_sessions+1},1,codex,working,-,0,0,,,,,,LOCAL,S1', 'CAPACITY')
            if args.demo: time.sleep(8)
            expect_ok(port, f'AT+DELETE={epoch},s4')
            session(f's{max_sessions+1}', 1, 'working', provider='codex')
            # Re-sync removes unseen entries but preserves a same-rev seen session.
            expect_ok(port, f'AT+SYNCBEGIN={epoch},2'); session('s2', 1, 'working', provider='codex')
            expect_ok(port, f'AT+SYNCEND={epoch},2')
            assert query_value(port, 'AT+LIST?', '+LIST:').split(',')[1] == '1'
            expect_ok(port, f'AT+SYNCEND={epoch},2')
            session('s2', 2, 'task_success', provider='codex')
            session('s2', 3, 'ready', provider='codex')
            reset = int(time.time())+7200
            for window, seconds in (('primary',18000),('secondary',604800)):
                expect_ok(port, f'AT+USAGE={epoch},codex,l1,{window},4200,{reset},{seconds},{int(time.time())},480')
            expect_ok(port, f'AT+USAGEBEGIN={epoch},codex,1')
            expect_ok(port, f'AT+USAGE={epoch},codex,l1,primary,4300,{reset},18000,{int(time.time())},480')
            expect_ok(port, f'AT+USAGEBEGIN={epoch},codex,1')  # retry preserves pending records
            expect_error(port, f'AT+USAGEEND={epoch},codex,2', 'BAD_STATE')
            expect_ok(port, f'AT+USAGEEND={epoch},codex,1')
            expect_ok(port, f'AT+USAGEEND={epoch},codex,1')
            for i in range(1, max_usages):
                expect_ok(port, f'AT+USAGE={epoch},codex,l1,w{i},4200,{reset},18000,{int(time.time())},480')
            expect_error(port, f'AT+USAGE={epoch},codex,l1,overflow,4200,{reset},18000,{int(time.time())},480', 'CAPACITY')
            expect_ok(port, f'AT+USAGE={epoch},codex,l1,primary,4400,{reset},18000,{int(time.time())},480')
            expect_error(port, f'AT+USAGEBEGIN={epoch},codex,0', 'INVALID_ARG')
            expect_ok(port, f'AT+USAGEBEGIN={epoch},codex,2')
            expect_error(port, f'AT+USAGE={epoch},codex,l1,primary,10001,0,18000,{int(time.time())},0', 'INVALID_ARG')
            expect_error(port, f'AT+USAGEEND={epoch},codex,2', 'BAD_STATE')
            expect_ok(port, f'AT+USAGEBEGIN={epoch},codex,3')
            for i in range(max_usages):
                expect_ok(port, f'AT+USAGE={epoch},codex,l1,w{i},4200,{reset},18000,{int(time.time())},480')
            expect_error(port, f'AT+USAGE={epoch},codex,l1,w{max_usages},4200,{reset},18000,{int(time.time())},480', 'CAPACITY')
            expect_error(port, f'AT+USAGEEND={epoch},codex,3', 'BAD_STATE')
            # Keep the display populated throughout repeated, deliberately slow refreshes.
            for batch in range(4, 12):
                expect_ok(port, f'AT+USAGEBEGIN={epoch},codex,{batch}')
                for window, seconds in (('primary', 18000), ('secondary', 604800)):
                    expect_ok(port, f'AT+USAGE={epoch},codex,l1,{window},{4200+batch},{reset},{seconds},{int(time.time())},480')
                    if args.demo: time.sleep(.3)
                expect_ok(port, f'AT+USAGEEND={epoch},codex,{batch}')
            if args.timeout:
                expect_ok(port, f'AT+USAGEBEGIN={epoch},codex,12')
                for i in range(7):
                    time.sleep(4.5)
                    expect_ok(port, 'AT+PING=' + secrets.token_hex(4))
                expect_error(port, f'AT+USAGEEND={epoch},codex,12', 'BAD_STATE')
                expect_error(port, f'AT+USAGE={epoch},codex,l1,primary,4200,{reset},18000,{int(time.time())},480', 'BAD_STATE')
                expect_ok(port, f'AT+USAGEBEGIN={epoch},codex,13')
                expect_ok(port, f'AT+USAGEEND={epoch},codex,13')
            if args.demo: time.sleep(2)
            expect_ok(port, f'AT+USAGECLEAR={epoch},codex')
            expect_ok(port, f'AT+USAGE={epoch},codex,l1,primary,-,{reset},18000,{int(time.time())},480')
            if args.demo: time.sleep(2)
            expect_ok(port, f'AT+USAGECLEAR={epoch},codex')
            expect_ok(port, f'AT+USAGE={epoch},codex,l1,primary,-,0,18000,{int(time.time())},-')
            expect_error(port, f'AT+USAGE={epoch},codex,l1,primary,-,0,18000,{int(time.time())}', 'INVALID_ARG')
            expect_error(port, f'AT+USAGE={epoch},codex,l1,primary,10001,0,18000,{int(time.time())},0', 'INVALID_ARG')
            if args.demo: time.sleep(2)
            rotation = query_value(port, 'AT+TFTROTATE?', '+TFTROTATE:')
            for angle in (0,90,180,270):
                expect_ok(port, f'AT+TFTROTATE={angle}')
                assert query_value(port, 'AT+TFTROTATE?', '+TFTROTATE:') == str(angle)
                if args.demo: time.sleep(1)
            expect_ok(port, f'AT+TFTROTATE={rotation}')
            expect_error(port, 'AT+' + 'X'*511, 'LINE_TOO_LONG')
            expect_ok(port, 'AT+INFO?')
            if args.timeout:
                print('Waiting 31 seconds for host timeout')
                time.sleep(31)
                assert query_value(port, 'AT+LIST?', '+LIST:').split(',')[1] == '0'
                assert query_value(port, 'AT+SHOWINFO?', '+SHOWINFO:') == '0'
            if args.factory:
                expect_ok(port, 'AT+MODE=FACTORY'); time.sleep(2.5)
                hello(port)
                expect_error(port, 'AT+SHOWINFO=10', 'UNSUPPORTED_FEATURE')
                expect_error(port, 'AT+SHOWINFO?', 'UNSUPPORTED_FEATURE')
                for command in ('AT+TESTLED=ALL,255,0,0,32','AT+TESTTFT=RED','AT+TESTBACKLIGHT=128',
                                *(f'AT+TESTFONT={f}' for f in ('8x8','8x16','10x18','12x22','16x32')),
                                'AT+TESTAUTO=1','AT+TESTAUTO=0'):
                    expect_ok(port, command); time.sleep(.2)
                expect_ok(port, 'AT+MODE=NORMAL'); time.sleep(2.5)
            # Leave a clean normal work flow and synced local clock for inspection.
            hello(port)
            offset = int(datetime.now().astimezone().utcoffset().total_seconds()/60)
            expect_ok(port, f'AT+TIME={int(time.time()*1000)},{offset}')
            synchronize(port, secrets.token_hex(16))
            expect_ok(port, 'AT+SHOWINFO=10')
            print('Phase-one UI serial checks passed; physical visuals still require inspection.')
        finally:
            if rotation is not None and query_value(port, 'AT+MODE?', '+MODE:') == 'NORMAL':
                expect_ok(port, 'AT+TFTROTATE=' + rotation)
            current = query_value(port, 'AT+MODE?', '+MODE:')
            if current != original_mode:
                expect_ok(port, 'AT+MODE=' + original_mode)


if __name__ == '__main__':
    main()
