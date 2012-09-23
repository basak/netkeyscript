#!/usr/bin/python

# Copyright 2012 Robie Basak
# 
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to
# deal in the Software without restriction, including without limitation the
# rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
# sell copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
# 
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
# 
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
# OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
# ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
# OTHER DEALINGS IN THE SOFTWARE.

NETKEYSCRIPT_PROTO_PASSPHRASE = 0

import argparse
import struct
import sys

from scapy.all import (
    Ether,
    IPv6,
    UDP,
    sendp
)


def send(dst, sport, dport, payload, iface):
    ether = Ether()
    ip = IPv6(dst=dst)
    udp = UDP(sport=sport, dport=dport)
    sendp(ether / ip / udp / payload, iface=iface)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--iface', default='eth0')
    parser.add_argument('--sport', default=30621, type=int)
    parser.add_argument('--dport', default=30621, type=int)
    parser.add_argument('--dest', default='ff02::1')
    args = parser.parse_args()

    payload_command = struct.pack('b', NETKEYSCRIPT_PROTO_PASSPHRASE)
    payload = payload_command + sys.stdin.read()
    send(dst=args.dest, sport=args.sport, dport=args.dport,
         payload=payload, iface=args.iface)


if __name__ == '__main__':
    main()
