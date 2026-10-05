#!/usr/bin/env python3

"""
Sign an RT58x firmware image for secure boot.

The image length is stored at offset 0x28, the image is signed with ECDSA P-256/SHA-256 and
the signature trailer is appended. rafael_ota_tool.py uses the functions of this module to
sign OTA images.

Usage example:
./rafael_sign_image.py -pk priv.pem -pub pub.pem my-firmware.bin

The output is written to my-firmware_Signature.bin unless an output file is given.
"""

import argparse
import os
import struct
import sys

try:
    from cryptography.exceptions import InvalidSignature
    from cryptography.hazmat.primitives import hashes, serialization
    from cryptography.hazmat.primitives.asymmetric import ec
    from cryptography.hazmat.primitives.asymmetric.utils import decode_dss_signature
except ImportError:
    ec = None

SBIN_MAGIC = 0x58BF4E53
SBIN_ALIGNMENT = 256
SBIN_TRAILER_SIZE = 256
ECDSA_P256_COORD_SIZE = 32

IMAGE_LENGTH_OFFSET = 0x28

MAX_INPUT_IMAGE_SIZE = 1984 * 1024


def warn(message: str):
    sys.stderr.write(f'warning: {message}\n')


def error(message: str):
    sys.stderr.write(f'error: {message}\n')
    sys.exit(1)


def signing_available() -> bool:
    """
    Return whether the "cryptography" Python package needed for signing is installed
    """

    return ec is not None


def load_private_key(path: str):
    """
    Load an ECDSA P-256 private key from a PEM file
    """

    with open(path, 'rb') as file:
        key = serialization.load_pem_private_key(file.read(), password=None)

    if not isinstance(key, ec.EllipticCurvePrivateKey) or not isinstance(key.curve, ec.SECP256R1):
        error(f'{path} is not an ECDSA P-256 (prime256v1) private key')

    return key


def load_public_key(path: str):
    """
    Load an ECDSA P-256 public key from a PEM file
    """

    with open(path, 'rb') as file:
        key = serialization.load_pem_public_key(file.read())

    if not isinstance(key, ec.EllipticCurvePublicKey) or not isinstance(key.curve, ec.SECP256R1):
        error(f'{path} is not an ECDSA P-256 (prime256v1) public key')

    return key


def patch_image_length(data: bytes) -> bytes:
    """
    Store the image length at IMAGE_LENGTH_OFFSET, which the bootloader checks before signature verification
    """

    if len(data) < IMAGE_LENGTH_OFFSET + 4:
        error('Input image is too small to hold the image length field')

    current = struct.unpack_from('<I', data, IMAGE_LENGTH_OFFSET)[0]
    if current not in (0, 0xFFFFFFFF, len(data)):
        warn(f'Overwriting non-empty value 0x{current:08x} at image length offset 0x{IMAGE_LENGTH_OFFSET:x}')

    patched = bytearray(data)
    struct.pack_into('<I', patched, IMAGE_LENGTH_OFFSET, len(data))
    return bytes(patched)


def sbin_signature_location(data_len: int) -> int:
    """
    Offset of the signature trailer for data of data_len bytes
    """

    return (data_len + SBIN_ALIGNMENT - 1) & ~(SBIN_ALIGNMENT - 1)


def sbin_size(data_len: int) -> int:
    """
    Size of data of data_len bytes after the signature trailer is appended
    """

    return sbin_signature_location(data_len) + SBIN_TRAILER_SIZE


def sbin_append_signature(data: bytes, r: int, s: int) -> bytes:
    """
    Append the ECDSA signature trailer to data (Python port of sbin_combine)
    """

    sig_location = sbin_signature_location(len(data))

    trailer = struct.pack('<II', SBIN_MAGIC, len(data))
    trailer += r.to_bytes(ECDSA_P256_COORD_SIZE, 'little')
    trailer += s.to_bytes(ECDSA_P256_COORD_SIZE, 'little')
    trailer += b'\xff' * (SBIN_TRAILER_SIZE - len(trailer))

    return data + b'\xff' * (sig_location - len(data)) + trailer


def sign_image(data: bytes, private_key, public_key, patch_length: bool) -> bytes:
    """
    Sign data with ECDSA P-256/SHA-256 and return it with the signature trailer appended.

    If public_key is given, the signature is verified with it before being appended.
    """

    if patch_length:
        data = patch_image_length(data)

    signature = private_key.sign(data, ec.ECDSA(hashes.SHA256()))

    if public_key is not None:
        try:
            public_key.verify(signature, data, ec.ECDSA(hashes.SHA256()))
        except InvalidSignature:
            error('Signature verification failed: public key does not match private key')

    r, s = decode_dss_signature(signature)
    return sbin_append_signature(data, r, s)


def default_output_path(input_path: str) -> str:
    """
    Return <input name>_Signature.bin next to the input file
    """

    return os.path.splitext(input_path)[0] + '_Signature.bin'


def sign_file(input_path: str, output_path: str, private_key_path: str, public_key_path) -> None:
    """
    Sign input_path and write the signed image to output_path
    """

    if not signing_available():
        error('Signing requires the "cryptography" Python package')

    if not os.path.isfile(input_path):
        error(f'{input_path} does not exist')

    signed_size = sbin_size(os.path.getsize(input_path))
    if signed_size > MAX_INPUT_IMAGE_SIZE:
        error(f'{input_path} is {signed_size} bytes after signing, larger than the '
              f'{MAX_INPUT_IMAGE_SIZE // 1024} KB ({MAX_INPUT_IMAGE_SIZE} bytes) limit')

    private_key = load_private_key(private_key_path)
    public_key = load_public_key(public_key_path) if public_key_path is not None else None

    with open(input_path, 'rb') as file:
        data = file.read()

    signed = sign_image(data, private_key, public_key, patch_length=True)

    with open(output_path, 'wb') as file:
        file.write(signed)

    print(f'Signed image   : {output_path} ({len(data)} -> {len(signed)} bytes)')
    print(f'Verified       : {"yes" if public_key is not None else "no (no public key given)"}')


def main():
    parser = argparse.ArgumentParser(description='Sign an RT58x firmware image for secure boot')
    parser.add_argument('-pk', '--private-key', required=True,
                        help='ECDSA P-256 private key (PEM) used to sign the image')
    parser.add_argument('-pub', '--public-key',
                        help='ECDSA P-256 public key (PEM) used to verify the signature after signing')
    parser.add_argument('input_file', help='Path to the firmware image to sign')
    parser.add_argument('output_file', nargs='?',
                        help='Path to the signed image (default: <input name>_Signature.bin)')

    args = parser.parse_args()

    output_path = args.output_file if args.output_file is not None else default_output_path(args.input_file)
    sign_file(args.input_file, output_path, args.private_key, args.public_key)


if __name__ == '__main__':
    main()
