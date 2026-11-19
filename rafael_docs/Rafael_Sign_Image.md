# Signing RT58x firmware images

`third_party/rafael/rafael_sign_image.py` signs an RT58x application image for
secure boot. It produces the same `*_Signature.bin` that
`rafael_ota_tool.py` creates internally when it builds a signed Matter OTA file.

## When to use it

On a chip whose secure boot public key has been programmed, the bootloader
verifies the signature of the application on every boot. If the application is
not signed with the matching private key, the bootloader does not start it and
the device stays in the bootloader.

- Use `rafael_sign_image.py` to sign an image that is flashed directly, for
  example with the Rafael ISP tool, onto a chip with secure boot enabled.
- To update a device over the air, see [Rafael_Software_Update.md](Rafael_Software_Update.md#create-signed-matter-ota-file).

## Prerequisites

- An ECDSA P-256 key pair. Generate it once and keep `private.pem` in a safe
  place outside the project directory. Anyone who has the private key can
  create images that your devices will accept.

      openssl ecparam -name prime256v1 -genkey -noout -out private.pem
      openssl ec -in private.pem -pubout -out public.pem

- The public key that belongs to `private.pem`, programmed into the secure boot
  info page of the chip with the Rafael ISP tool.

- The activated build environment, which provides the `cryptography` Python
  package used for signing.

      source ./scripts/activate.sh

- `rafael_sign_image.py` and `rafael_ota_tool.py` are kept in the same
  directory, `third_party/rafael/`. `rafael_ota_tool.py` imports its signing
  functions from `rafael_sign_image.py`.

## Usage

    ./third_party/rafael/rafael_sign_image.py -pk <private key> [-pub <public key>] <input image> [<output image>]

| Argument              | Required | Description                                                                         |
| --------------------- | -------- | ----------------------------------------------------------------------------------- |
| `-pk`, `--private-key` | Yes      | ECDSA P-256 private key (PEM) used to sign the image                                |
| `-pub`, `--public-key` | No       | ECDSA P-256 public key (PEM). When given, the signature is verified right after signing |
| `<input image>`       | Yes      | Application image from the build, for example `out/<project>/<board>/<board>-<project>-example.bin` |
| `<output image>`      | No       | Signed image. Defaults to `<input name>_Signature.bin` next to the input image      |

Passing `-pub` is recommended. It catches a private key that does not match the
public key before the image is flashed.

The input image is not modified.

## Example

    ./third_party/rafael/rafael_sign_image.py -pk private.pem -pub public.pem out/lighting-app/RT584HA4/RT584HA4-lighting-app-example.bin

Output:

    Signed image   : out/lighting-app/RT584HA4/RT584HA4-lighting-app-example_Signature.bin (919476 -> 919808 bytes)
    Verified       : yes

## Errors

The tool prints an error and exits with status 1 without writing the output
image when:

| Message                                                            | Cause                                                       |
| ------------------------------------------------------------------ | ----------------------------------------------------------- |
| `the following arguments are required: -pk/--private-key`          | No private key was given                                    |
| `<file> does not exist`                                            | The input image path is wrong                               |
| `<file> is <size> bytes after signing, larger than the 1984 KB ...` | The image is too large                                      |
| `<file> is not an ECDSA P-256 (prime256v1) private key`            | The key is not an ECDSA P-256 key                           |
| `Signature verification failed: public key does not match private key` | `-pub` and `-pk` are not a key pair                     |
| `Signing requires the "cryptography" Python package`               | The build environment is not activated                      |

If the build environment is not activated, the system Python may also fail with
a Python traceback instead of one of these messages. Run
`source ./scripts/activate.sh` and try again.
