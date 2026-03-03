#!/usr/bin/env python3
import sys
import struct
import hashlib
import argparse
import time
from binascii import hexlify, unhexlify


DEFAULT_CHALLENGE = "1122334455667788"


class DES:
    _IP = [58,50,42,34,26,18,10,2,60,52,44,36,28,20,12,4,
           62,54,46,38,30,22,14,6,64,56,48,40,32,24,16,8,
           57,49,41,33,25,17,9,1,59,51,43,35,27,19,11,3,
           61,53,45,37,29,21,13,5,63,55,47,39,31,23,15,7]
    _FP = [40,8,48,16,56,24,64,32,39,7,47,15,55,23,63,31,
           38,6,46,14,54,22,62,30,37,5,45,13,53,21,61,29,
           36,4,44,12,52,20,60,28,35,3,43,11,51,19,59,27,
           34,2,42,10,50,18,58,26,33,1,41,9,49,17,57,25]
    _PC1 = [57,49,41,33,25,17,9,1,58,50,42,34,26,18,10,2,
            59,51,43,35,27,19,11,3,60,52,44,36,63,55,47,39,
            31,23,15,7,62,54,46,38,30,22,14,6,61,53,45,37,
            29,21,13,5,28,20,12,4]
    _PC2 = [14,17,11,24,1,5,3,28,15,6,21,10,23,19,12,4,
            26,8,16,7,27,20,13,2,41,52,31,37,47,55,30,40,
            51,45,33,48,44,49,39,56,34,53,46,42,50,36,29,32]
    _E = [32,1,2,3,4,5,4,5,6,7,8,9,8,9,10,11,12,13,
          12,13,14,15,16,17,16,17,18,19,20,21,20,21,22,23,
          24,25,24,25,26,27,28,29,28,29,30,31,32,1]
    _P = [16,7,20,21,29,12,28,17,1,15,23,26,5,18,31,10,
          2,8,24,14,32,27,3,9,19,13,30,6,22,11,4,25]
    _S = [
        [14,4,13,1,2,15,11,8,3,10,6,12,5,9,0,7,0,15,7,4,14,2,13,1,10,6,12,11,9,5,3,8,4,1,14,8,13,6,2,11,15,12,9,7,3,10,5,0,15,12,8,2,4,9,1,7,5,11,3,14,10,0,6,13],
        [15,1,8,14,6,11,3,4,9,7,2,13,12,0,5,10,3,13,4,7,15,2,8,14,12,0,1,10,6,9,11,5,0,14,7,11,10,4,13,1,5,8,12,6,9,3,2,15,13,8,10,1,3,15,4,2,11,6,7,12,0,5,14,9],
        [10,0,9,14,6,3,15,5,1,13,12,7,11,4,2,8,13,7,0,9,3,4,6,10,2,8,5,14,12,11,15,1,13,6,4,9,8,15,3,0,11,1,2,12,5,10,14,7,1,10,13,0,6,9,8,7,4,15,14,3,11,5,2,12],
        [7,13,14,3,0,6,9,10,1,2,8,5,11,12,4,15,13,8,11,5,6,15,0,3,4,7,2,12,1,10,14,9,10,6,9,0,12,11,7,13,15,1,3,14,5,2,8,4,3,15,0,6,10,1,13,8,9,4,5,11,12,7,2,14],
        [2,12,4,1,7,10,11,6,8,5,3,15,13,0,14,9,14,11,2,12,4,7,13,1,5,0,15,10,3,9,8,6,4,2,1,11,10,13,7,8,15,9,12,5,6,3,0,14,11,8,12,7,1,14,2,13,6,15,0,9,10,4,5,3],
        [12,1,10,15,9,2,6,8,0,13,3,4,14,7,5,11,10,15,4,2,7,12,9,5,6,1,13,14,0,11,3,8,9,14,15,5,2,8,12,3,7,0,4,10,1,13,11,6,4,3,2,12,9,5,15,10,11,14,1,7,6,0,8,13],
        [4,11,2,14,15,0,8,13,3,12,9,7,5,10,6,1,13,0,11,7,4,9,1,10,14,3,5,12,2,15,8,6,1,4,11,13,12,3,7,14,10,15,6,8,0,5,9,2,6,11,13,8,1,4,10,7,9,5,0,15,14,2,3,12],
        [13,2,8,4,6,15,11,1,10,9,3,14,5,0,12,7,1,15,13,8,10,3,7,4,12,5,6,11,0,14,9,2,7,11,4,1,9,12,14,2,0,6,10,13,15,3,5,8,2,1,14,7,4,10,8,13,15,12,9,0,3,5,6,11],
    ]
    _SHIFTS = [1,1,2,2,2,2,2,2,1,2,2,2,2,2,2,1]

    @staticmethod
    def _permute(block, table, in_bits):
        result = 0
        for i, pos in enumerate(table):
            if (block >> (in_bits - pos)) & 1:
                result |= 1 << (len(table) - 1 - i)
        return result

    @classmethod
    def _subkeys(cls, key_64bit):
        cd = cls._permute(key_64bit, cls._PC1, 64)
        c, d = cd >> 28, cd & 0xFFFFFFF
        subkeys = []
        for shift in cls._SHIFTS:
            c = ((c << shift) | (c >> (28 - shift))) & 0xFFFFFFF
            d = ((d << shift) | (d >> (28 - shift))) & 0xFFFFFFF
            subkeys.append(cls._permute((c << 28) | d, cls._PC2, 56))
        return subkeys

    @classmethod
    def _feistel(cls, r, subkey):
        expanded = cls._permute(r, cls._E, 32)
        xored = expanded ^ subkey
        s_out = 0
        for i in range(8):
            chunk = (xored >> ((7 - i) * 6)) & 0x3F
            row = ((chunk & 0x20) >> 4) | (chunk & 1)
            col = (chunk >> 1) & 0xF
            s_out = (s_out << 4) | cls._S[i][row * 16 + col]
        return cls._permute(s_out, cls._P, 32)

    @classmethod
    def encrypt(cls, plaintext_bytes: bytes, key_bytes: bytes) -> bytes:
        pt = int.from_bytes(plaintext_bytes, 'big')
        key = int.from_bytes(key_bytes, 'big')
        subkeys = cls._subkeys(key)
        ip = cls._permute(pt, cls._IP, 64)
        l, r = ip >> 32, ip & 0xFFFFFFFF
        for i in range(16):
            l, r = r, l ^ cls._feistel(r, subkeys[i])
        ct = cls._permute((r << 32) | l, cls._FP, 64)
        return ct.to_bytes(8, 'big')

def md4(data: bytes) -> bytes:
    def rotl(x, n):
        return ((x << n) | (x >> (32 - n))) & 0xFFFFFFFF
    def add32(*args):
        return sum(args) & 0xFFFFFFFF

    bit_len = len(data) * 8
    data += b"\x80"
    while len(data) % 64 != 56:
        data += b"\x00"
    data += struct.pack("<Q", bit_len)

    A, B, C, D = 0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476

    for i in range(0, len(data), 64):
        X = list(struct.unpack("<16I", data[i:i+64]))
        a, b, c, d = A, B, C, D
        for k, s in [(0,3),(1,7),(2,11),(3,19),(4,3),(5,7),(6,11),(7,19),
                      (8,3),(9,7),(10,11),(11,19),(12,3),(13,7),(14,11),(15,19)]:
            a = rotl(add32(a, (b & c) | (~b & d), X[k]), s)
            a, b, c, d = d, a, b, c
        for k, s in [(0,3),(4,5),(8,9),(12,13),(1,3),(5,5),(9,9),(13,13),
                      (2,3),(6,5),(10,9),(14,13),(3,3),(7,5),(11,9),(15,13)]:
            a = rotl(add32(a, (b & c) | (b & d) | (c & d), X[k], 0x5A827999), s)
            a, b, c, d = d, a, b, c
        for k, s in [(0,3),(8,9),(4,11),(12,15),(2,3),(10,9),(6,11),(14,15),
                      (1,3),(9,9),(5,11),(13,15),(3,3),(11,9),(7,11),(15,15)]:
            a = rotl(add32(a, b ^ c ^ d, X[k], 0x6ED9EBA1), s)
            a, b, c, d = d, a, b, c
        A, B, C, D = add32(A, a), add32(B, b), add32(C, c), add32(D, d)

    return struct.pack("<4I", A, B, C, D)


def nt_hash(password: str) -> bytes:
    return md4(password.encode("utf-16-le"))


def expand_des_key(seven_bytes: bytes) -> bytes:
    """Expand 7-byte key material to 8-byte DES key (NTLM-style with parity)."""
    n = seven_bytes
    return bytes([
        n[0] | 1,
        (((n[0] << 7) | (n[1] >> 1)) & 0xFF) | 1,
        (((n[1] << 6) | (n[2] >> 2)) & 0xFF) | 1,
        (((n[2] << 5) | (n[3] >> 3)) & 0xFF) | 1,
        (((n[3] << 4) | (n[4] >> 4)) & 0xFF) | 1,
        (((n[4] << 3) | (n[5] >> 5)) & 0xFF) | 1,
        (((n[5] << 2) | (n[6] >> 6)) & 0xFF) | 1,
        ((n[6] << 1) & 0xFF) | 1,
    ])


def des_encrypt(key_7: bytes, plaintext: bytes) -> bytes:
    return DES.encrypt(plaintext, expand_des_key(key_7))


def md5(data: bytes) -> bytes:
    return hashlib.md5(data).digest()

def cmd_parse(args):
    parts = args.response.split(":")
    if len(parts) < 6:
        print("Error: Expected user::DOMAIN:LMResponse:NTResponse:challenge", file=sys.stderr)
        sys.exit(1)

    user, domain = parts[0], parts[2]
    lm_response = parts[3].upper()
    nt_response = parts[4].upper()
    challenge = parts[5].upper()
    ess = parts[6].upper() if len(parts) > 6 and parts[6] else None

    if len(nt_response) != 48:
        print(f"Error: NTResponse must be 48 hex chars, got {len(nt_response)}", file=sys.stderr)
        sys.exit(1)
    if len(challenge) != 16:
        print(f"Error: Challenge must be 16 hex chars, got {len(challenge)}", file=sys.stderr)
        sys.exit(1)

    ct1, ct2, ct3 = nt_response[0:16], nt_response[16:32], nt_response[32:48]

    print(f"User:          {user}")
    print(f"Domain:        {domain}")
    print(f"Challenge:     {challenge}")
    print()
    print(f"LM Response:   {lm_response}")
    print(f"NT Response:   {nt_response}")
    print()
    print(f"CT1:           {ct1}")
    print(f"CT2:           {ct2}")
    print(f"CT3:           {ct3}")
    if ess:
        print(f"ESS:           {ess}")

    if challenge == DEFAULT_CHALLENGE:
        print(f"\n[+] Static challenge - compatible with rainbow tables")
    elif ess:
        ess_bytes = unhexlify(ess)
        if all(b == 0 for b in ess_bytes[8:]):
            effective = hexlify(md5(unhexlify(challenge) + ess_bytes[:8])[:8]).decode().upper()
            print(f"\n[*] ESS detected - effective challenge: {effective}")
            if effective == DEFAULT_CHALLENGE:
                print(f"[+] Effective challenge matches static challenge")
            else:
                print(f"[-] Effective challenge does NOT match {DEFAULT_CHALLENGE}")
        else:
            print(f"\n[-] Challenge is not {DEFAULT_CHALLENGE}")
    else:
        print(f"\n[-] Challenge is not {DEFAULT_CHALLENGE}")
        print(f"    Rainbow tables require the static challenge")


def cmd_pw2nt(args):
    nt = hexlify(nt_hash(args.password)).decode().upper()
    print(f"Password:  {args.password}")
    print(f"NT Hash:   {nt}")


def cmd_nt2ct(args):
    nt_hex = args.nt_hash.upper()
    challenge_hex = args.challenge.upper()

    if len(nt_hex) != 32 or not all(c in "0123456789ABCDEF" for c in nt_hex):
        print("Error: NT hash must be 32 hex characters", file=sys.stderr)
        sys.exit(1)

    nt_bytes = unhexlify(nt_hex)
    challenge = unhexlify(challenge_hex)

    ct1 = hexlify(des_encrypt(nt_bytes[0:7], challenge)).decode().upper()
    ct2 = hexlify(des_encrypt(nt_bytes[7:14], challenge)).decode().upper()
    ct3 = hexlify(des_encrypt(nt_bytes[14:16] + b"\x00" * 5, challenge)).decode().upper()

    print(f"NT Hash:       {nt_hex}")
    print(f"Challenge:     {challenge_hex}")
    print(f"CT1:           {ct1}")
    print(f"CT2:           {ct2}")
    print(f"CT3:           {ct3}")
    print(f"Full Response: {ct1}{ct2}{ct3}")


def cmd_ct3(args):
    ct3_hex = args.ct3.upper()
    challenge_hex = args.challenge.upper()

    if len(ct3_hex) != 16 or not all(c in "0123456789ABCDEF" for c in ct3_hex):
        print("Error: CT3 must be 16 hex characters", file=sys.stderr)
        sys.exit(1)

    ct3_target = unhexlify(ct3_hex)
    challenge = unhexlify(challenge_hex)

    if args.ess:
        ess_bytes = unhexlify(args.ess)
        if all(b == 0 for b in ess_bytes[8:]):
            challenge = md5(challenge + ess_bytes[:8])[:8]

    print("Searching 65,536 keys...")
    start = time.time()

    for i in range(0x10000):
        key_7 = bytes([i & 0xFF, (i >> 8) & 0xFF, 0, 0, 0, 0, 0])
        if des_encrypt(key_7, challenge) == ct3_target:
            elapsed = time.time() - start
            key_hex = f"{i & 0xFF:02X}{(i >> 8) & 0xFF:02X}"
            print(f"Key3: {key_hex} ({elapsed*1000:.0f}ms)")
            return

    print(f"Key not found ({time.time() - start:.1f}s)")
    sys.exit(1)


def cmd_reconstruct(args):
    k1, k2, k3 = args.key1.upper(), args.key2.upper(), args.key3.upper()

    if len(k1) != 14 or not all(c in "0123456789ABCDEF" for c in k1):
        print("Error: Key1 must be 14 hex characters", file=sys.stderr)
        sys.exit(1)
    if len(k2) != 14 or not all(c in "0123456789ABCDEF" for c in k2):
        print("Error: Key2 must be 14 hex characters", file=sys.stderr)
        sys.exit(1)
    if len(k3) != 4 or not all(c in "0123456789ABCDEF" for c in k3):
        print("Error: Key3 must be 4 hex characters", file=sys.stderr)
        sys.exit(1)

    print(f"Key1:      {k1}")
    print(f"Key2:      {k2}")
    print(f"Key3:      {k3}")
    print(f"NT Hash:   {k1}{k2}{k3}")

def main():
    parser = argparse.ArgumentParser(description="DEStroy Utilities - NTLMv1 offline conversion tools")
    sub = parser.add_subparsers(dest="command", required=True)

    p = sub.add_parser("parse-ntlmv1", help="Parse an NTLMv1 response string")
    p.add_argument("response", help="user::DOMAIN:LMResponse:NTResponse:challenge")

    p = sub.add_parser("password-to-nt", help="Convert a password to NT hash")
    p.add_argument("password", help="Password to hash")

    p = sub.add_parser("nt-to-ct", help="Convert an NT hash to DES ciphertexts")
    p.add_argument("nt_hash", help="NT hash (32 hex chars)")
    p.add_argument("-c", "--challenge", default=DEFAULT_CHALLENGE, help="Server challenge")

    p = sub.add_parser("recover-ct3", help="Recover CT3 DES key (bruteforce 16-bit keyspace)")
    p.add_argument("ct3", help="CT3 ciphertext (16 hex chars)")
    p.add_argument("-c", "--challenge", default=DEFAULT_CHALLENGE, help="Server challenge")
    p.add_argument("-e", "--ess", default=None, help="ESS value (48 hex chars)")

    p = sub.add_parser("recover-nt", help="Recover NT hash from three DES keys")
    p.add_argument("key1", help="First DES key (14 hex chars)")
    p.add_argument("key2", help="Second DES key (14 hex chars)")
    p.add_argument("key3", help="Third DES key (4 hex chars)")

    args = parser.parse_args()
    {"parse-ntlmv1": cmd_parse, "password-to-nt": cmd_pw2nt, "nt-to-ct": cmd_nt2ct,
     "recover-ct3": cmd_ct3, "recover-nt": cmd_reconstruct}[args.command](args)


if __name__ == "__main__":
    main()
