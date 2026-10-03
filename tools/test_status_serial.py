#!/usr/bin/env python3
"""Verify rendered status animation frames for all eight session states on hardware."""
import argparse
import secrets
import time
import serial
from test_at_serial import expect_ok, query_value
from test_ui_serial import hello, synchronize
from test_idle_serial import diagnostic


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('port')
    args = parser.parse_args()
    states = ['working','thinking','waiting_for_input','permission_required',
              'stale','task_success','task_failed','task_canceled']
    colors = [0x36e29a,0x8b99ff,0xffb02e,0xff5f7a,0xa3afd3,0x36e29a,0xff3b30,0x4aa8ff]
    periods = [24,40,20,20,30,40,24,40]
    with serial.Serial(args.port, 115200, timeout=.1, write_timeout=2) as port:
        port.dtr = False
        port.rts = False
        time.sleep(2.2)
        port.reset_input_buffer()
        assert query_value(port,'AT+MODE?','+MODE:') == 'NORMAL'
        logs = query_value(port,'AT+LOG?','+LOG:')
        try:
            hello(port)
            epoch = secrets.token_hex(16)
            synchronize(port,epoch)
            expect_ok(port,'AT+LOG=1')
            for index, state in enumerate(states):
                result = {'task_success':'completed','task_failed':'failed','task_canceled':'cancelled'}.get(state,'-')
                finished = int(time.time()) if result != '-' else 0
                expect_ok(port,f'AT+SESSION={epoch},motion,{index+1},codex,{state},{result},{finished},725,,,,,status_motion')
                start = time.monotonic()
                frames, sequences = set(), set()
                while time.monotonic() - start < 2.5:
                    row = diagnostic(port)
                    if row['status'] != index:
                        assert time.monotonic()-start < .5, row
                        continue
                    assert 0 <= row['frame'] < periods[index] and row['rgb'] == colors[index], row
                    frames.add(row['frame']); sequences.add(row['status_seq'])
                    time.sleep(.07)
                assert len(frames) >= 8 and len(sequences) >= 8, (state,frames,sequences)
                print(f'PASS {state}: {len(frames)} distinct successfully rendered frames; diagnostics {row}',flush=True)
            expect_ok(port,f'AT+DELETE={epoch},motion')
            time.sleep(.3)
            row = diagnostic(port)
            assert row['status'] == 255 and row['frame'] == 255 and row['mask'] == 0, row
            print('Status animation serial checks passed; physical diffusion and color still need visual inspection.',flush=True)
        finally:
            hello(port)
            synchronize(port,secrets.token_hex(16))
            expect_ok(port,'AT+LOG='+logs)


if __name__ == '__main__':
    main()
