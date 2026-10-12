#!/usr/bin/env python3
"""Exercise AT + XMODEM OTA on an explicitly selected, released UART port."""
import argparse
import binascii
import hashlib
from pathlib import Path
import re
import shutil
import subprocess
import time

import serial

ACK, NAK, CAN, EOT, CRC = 6, 0x15, 0x18, 4, ord('C')


def terminal(port, timeout=10):
    """Read bytewise so a following XMODEM 'C' remains in the serial buffer."""
    deadline = time.monotonic() + timeout
    pending = bytearray()
    lines = []
    while time.monotonic() < deadline:
        byte = port.read(1)
        if not byte:
            continue
        if byte == b'\n':
            line = pending.rstrip(b'\r').decode('ascii', errors='replace')
            pending.clear()
            if line:
                lines.append(line)
            if line in ('OK', 'ERROR'):
                return line, lines
        elif byte[0] >= 32 or byte == b'\r':
            pending.extend(byte)
    raise TimeoutError(f'No terminal AT response; received {lines!r}')


def exchange(port, command, timeout=10):
    port.write(command.encode('ascii') + b'\r\n')
    final, lines = terminal(port, timeout)
    print(command.split('=', 1)[0], '->', lines)
    if final != 'OK':
        raise RuntimeError(f'{command}: {lines}')
    return lines


def control(port, timeout=5):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        byte = port.read(1)
        if byte and byte[0] in (ACK, NAK, CAN, CRC):
            return byte[0]
    raise TimeoutError('No XMODEM control byte')


def send_packet(port, packet):
    for _ in range(10):
        port.write(packet)
        answer = control(port)
        if answer == ACK:
            return
        if answer == CAN:
            raise RuntimeError('Device cancelled XMODEM; wait for its terminal AT error')
    raise RuntimeError('XMODEM retransmission limit')


def upload(port, image, block_size, fault):
    if fault == 'short' and len(image) <= block_size:
        port.write(bytes([EOT]))
        return
    for index, offset in enumerate(range(0, len(image), block_size), 1):
        block = image[offset:offset + block_size].ljust(block_size, b'\x1a')
        number = index & 255
        packet = bytes([1 if block_size == 128 else 2, number, number ^ 255]) + block
        packet += binascii.crc_hqx(block, 0).to_bytes(2, 'big')
        if index == 1 and fault == 'crc':
            port.write(packet[:-1] + bytes([packet[-1] ^ 1]))
            if control(port) != NAK:
                raise AssertionError('Corrupt CRC was not rejected')
        send_packet(port, packet)
        if index == 1 and fault == 'retry':
            send_packet(port, packet)
        if index % 64 == 0:
            print(f'{min(offset + block_size, len(image))}/{len(image)} bytes')
        if fault == 'short':
            port.write(bytes([EOT]))
            return
    for _ in range(10):
        port.write(bytes([EOT]))
        answer = control(port)
        if answer == ACK:
            return
        if answer == CAN:
            raise RuntimeError('Device cancelled at EOT')
    raise RuntimeError('EOT not acknowledged')


def check_port_available(port):
    if shutil.which('lsof'):
        result = subprocess.run(['lsof', '-t', port], capture_output=True, text=True)
        if result.returncode == 0 and result.stdout.strip():
            raise RuntimeError(f'{port} is occupied by PID(s) {result.stdout.strip()}; release the port before testing')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('port', help='Actual CH340 UART port; no auto detection or host-service shutdown')
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument('--firmware', type=Path, help='Application .bin (never a merged Flash image)')
    mode.add_argument('--cancel', action='store_true', help='Enter reception and send CAN CAN; erases the unused OTA slot')
    mode.add_argument('--timeout', action='store_true', help='Enter reception and let the 30-second handshake expire')
    parser.add_argument('--version', help='Target version; otherwise read from ESP application descriptor')
    parser.add_argument('--block-size', type=int, choices=[128, 1024], default=1024)
    parser.add_argument('--fault', choices=['crc', 'retry', 'hash', 'version', 'short'])
    parser.add_argument('--commit', action='store_true', help='Explicitly activate a successfully uploaded image and reboot')
    args = parser.parse_args()
    if args.commit and (not args.firmware or args.fault in ('hash', 'version', 'short')):
        parser.error('--commit requires a firmware upload that is expected to reach READY')
    if (args.fault or args.version) and not args.firmware:
        parser.error('--fault and --version require --firmware')
    image = args.firmware.read_bytes() if args.firmware else b''
    version = args.version
    if image:
        if len(image) < 288 or image[0] != 0xe9:
            parser.error('Expected an ESP application .bin with an application descriptor')
        if image[32:36] != b'\x32\x54\xcd\xab' or image[80:112].split(b'\0', 1)[0] != b'agentmate_esp32':
            parser.error('Application descriptor is missing or belongs to another project')
        if not version:
            version = 'v' + image[48:80].split(b'\0', 1)[0].decode('ascii')
        if not re.fullmatch(r'v[0-9]+\.[0-9]+', version):
            parser.error('Expected version v<major>.<minor>')
        if args.fault == 'version':
            version = 'v999.999' if version != 'v999.999' else 'v998.999'
    elif args.firmware:
        parser.error('Firmware is empty')
    check_port_available(args.port)
    # Set DTR/RTS before opening to avoid intentionally resetting the board.
    port = serial.Serial(port=None, baudrate=115200, timeout=0.1, write_timeout=5, exclusive=True)
    port.dtr = False
    port.rts = False
    port.port = args.port
    port.open()
    binary = False
    try:
        # Some USB-UART bridges reset the board on open; wait for its AT task and discard ROM bytes.
        startup_deadline = time.monotonic() + 2.2
        while time.monotonic() < startup_deadline:
            port.read(256)
        port.reset_input_buffer()
        exchange(port, 'AT+INFO?')
        exchange(port, 'AT+OTA?')
        if not (args.firmware or args.cancel or args.timeout):
            return
        exchange(port, 'AT+OTAABORT')
        original_version = next(line.split(',')[2] for line in exchange(port, 'AT+INFO?') if line.startswith('+INFO:'))
        size = len(image) if image else 512
        digest = hashlib.sha256(image).hexdigest() if image else '0' * 64
        if args.fault == 'hash':
            digest = ('1' if digest[0] == '0' else '0') + digest[1:]
        exchange(port, f'AT+OTABEGIN={version or "v2.1"},{size},{digest}', timeout=30)
        binary = True
        if control(port) != CRC:
            raise AssertionError('Expected CRC handshake C')
        expected_error = None
        if args.cancel:
            port.write(bytes([CAN, CAN]))
            expected_error = 'OTA_CANCELLED'
        elif args.timeout:
            expected_error = 'OTA_TIMEOUT'
        else:
            upload(port, image, args.block_size, args.fault)
            expected_error = {'hash': 'OTA_HASH', 'version': 'OTA_IDENTITY', 'short': 'OTA_SIZE'}.get(args.fault)
        final, lines = terminal(port, timeout=40)
        binary = False
        print('Transfer result:', lines)
        if expected_error:
            assert final == 'ERROR' and f'+ERR:{expected_error}' in lines, lines
            exchange(port, 'AT+OTA?')
            exchange(port, 'AT+OTAABORT')
            exchange(port, 'AT+INFO?')
            print('PASS failure recovered to AT; running image remains', original_version)
        else:
            assert final == 'OK' and any(line.startswith('+OTA:READY,') for line in lines), lines
            exchange(port, 'AT+OTA?')
            if args.commit:
                exchange(port, 'AT+OTACOMMIT')
                # Wait past the boot self-test; discard ROM reset bytes before discovery.
                deadline = time.monotonic() + 20
                while time.monotonic() < deadline:
                    port.read(256)
                port.reset_input_buffer()
                info = exchange(port, 'AT+INFO?')
                assert any(line.startswith('+INFO:') and line.split(',')[2] == version for line in info), info
                print('PASS committed and rediscovered', version)
            else:
                print('PASS READY; image awaits AT+OTACOMMIT or AT+OTAABORT')
    except (Exception, KeyboardInterrupt):
        # Best-effort cancellation if a sender stops mid-transfer. AT cannot abort binary mode.
        if binary:
            port.write(bytes([CAN, CAN]))
        raise
    finally:
        port.close()


if __name__ == '__main__':
    main()
