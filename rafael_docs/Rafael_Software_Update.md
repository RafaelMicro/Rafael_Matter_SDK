# Rafael RT58x Matter OTA example

## Build Matter OTA example

- On a Linux platform build the lighting-app (OTA is enabled by default for RT58x examples) as follow

      ./scripts/examples/gn_rt58x_example.sh ./examples/lighting-app/rt58x ./out/lighting-app RT584HA4

## Create Matter OTA file

Every OTA file created by the Rafael OTA tool, signed or not, starts its payload
with a 16-byte Rafael image header (see
[Create signed Matter OTA file](#create-signed-matter-ota-file) for its
layout). The device rejects OTA files without this header at the start of the
download, so devices must run firmware that supports this header before they can
be updated with files from this version of the tool.

- On a Linux platform create the Matter OTA file using Rafael OTA tool

      ./third_party/rafael/rafael_ota_tool.py create -v 0x1346 -p 0x400E -vn 2 -vs "2.0" -da sha256 out/lighting-app/RT584HA4/RT584HA4-lighting-app-example.bin out/lighting-app/RT584HA4/RT584HA4-lighting-app-example.ota
        
- To verify the Matter OTA file, run the script with following command
        
      ./third_party/rafael/rafael_ota_tool.py show out/lighting-app/RT584HA4/RT584HA4-lighting-app-example.ota

- Example output. `Image Info: 0x01` shows that the image is compressed but not
  signed.

      Magic: 1beef11e
      Total Size: 530101
      Header Size: 63
      Header TLV:
        [0] Vendor Id: 4934 (0x1346)
        [1] Product Id: 16398 (0x400e)
        [2] Version: 2 (0x2)
        [3] Version String: 2.0
        [4] Payload Size: 530022 (0x81666)
        [8] Digest Type: 1 (0x1)
        [9] Digest: e3816dcd3b1a6d209bed9868f62b16a1291cba1d7dcf0d90944a122585a803a3
      Rafael Header:
        Version: 1
        Header Size: 16
        Image Info: 0x01 (compressed: True, signed: False)
        Signature Length: 0 (0x0)
        Image Length: 530006 (0x81656)

## Create signed Matter OTA file

A signed OTA image lets the bootloader check, with ECDSA P-256, that the image
was released by the owner of the signing key and was not modified. The
bootloader only checks signatures on chips whose secure boot public key has been
programmed; other chips update without any signature check.

- Generate an ECDSA P-256 key pair once, and keep `private.pem` in a safe place
  outside the project directory. Anyone who has the private key can create
  images that your devices will accept.

      openssl ecparam -name prime256v1 -genkey -noout -out private.pem
      openssl ec -in private.pem -pubout -out public.pem

- Program the public key that belongs to `private.pem` into the secure boot info
  page of the chip with the Rafael MP tool. The bootloader verifies the image
  with this key.

- Activate the build environment first, since signing needs the `cryptography`
  Python package that it provides.

      source ./scripts/activate.sh

- Create the signed Matter OTA file by passing the private key with `-pk`.

      ./third_party/rafael/rafael_ota_tool.py create -v 0x1346 -p 0x400E -vn 2 -vs 2.0 -da sha256 -pk private.pem -pub public.pem out/lighting-app/RT584HA4/RT584HA4-lighting-app-example.bin out/lighting-app/RT584HA4/RT584HA4-lighting-app-example.ota

  - `-pk`: private key used to sign the image. Without it, the image is not signed.
  - `-pub`: optional public key. When given, the tool verifies both signatures
    right after signing, so a private key that does not match the public key is
    caught before the image is released.

- The tool signs the image, compresses it with LZMA and signs the compressed
  data again. The following intermediate files are written next to the `.ota`
  file for inspection. Only the `.ota` file is used for the Matter OTA.

  | File                                 | Content                                         |
  | ------------------------------------ | ----------------------------------------------- |
  | `*_Signature.bin`                    | Application image with its signature appended   |
  | `*_Signature_compress.bin`           | LZMA compressed `*_Signature.bin`               |
  | `*_Signature_compress_Signature.bin` | Compressed image with its signature appended    |

- The `.ota` file consists of the Matter header, a 16-byte Rafael image header
  and `*_Signature_compress_Signature.bin`. To verify it, run

      ./third_party/rafael/rafael_ota_tool.py show out/lighting-app/RT584HA4/RT584HA4-lighting-app-example.ota

- Example output. `Image Info: 0x03` shows that the image is signed.

      Magic: 1beef11e
      Total Size: 530527
      Header Size: 63
      Header TLV:
        [0] Vendor Id: 4934 (0x1346)
        [1] Product Id: 16398 (0x400e)
        [2] Version: 2 (0x2)
        [3] Version String: 2.0
        [4] Payload Size: 530448 (0x81810)
        [8] Digest Type: 1 (0x1)
        [9] Digest: 918f94300a94a4f2674f57d763c0e8c964011fcf07993340a539f6237281c298
      Rafael Header:
        Version: 1
        Header Size: 16
        Image Info: 0x03 (compressed: True, signed: True)
        Signature Length: 530084 (0x816a4)
        Image Length: 530432 (0x81800)

- The Rafael image header (all fields little endian) tells the device how to
  hand the image to the bootloader:

  | Offset | Size | Field            | Value                                                          |
  | ------ | ---- | ---------------- | -------------------------------------------------------------- |
  | 0      | 4    | Magic            | `0x544F4652` (`"RFOT"`)                                        |
  | 4      | 1    | Version          | 1                                                              |
  | 5      | 1    | Header size      | 16                                                             |
  | 6      | 1    | Image info       | `0x01` compressed, `0x03` compressed and signed                |
  | 7      | 1    | Reserved         | 0                                                              |
  | 8      | 4    | Signature length | Length of the compressed image covered by the outer signature |
  | 12     | 4    | Image length     | Number of image bytes after the header                         |

## Building OTA Provider on Raspberrypi

- First, clone official Matter repostory with following command:

      git clone --recurse-submodules git@github.com:project-chip/connectedhomeip.git
      
- Follow the [link](https://github.com/project-chip/connectedhomeip/blob/master/docs/guides/BUILDING.md#installing-prerequisites-on-raspberry-pi-4) to install the prerequisites of Raspberrypi.

- Follow the [link](https://github.com/project-chip/connectedhomeip/blob/master/docs/guides/BUILDING.md#prepare-for-building) to prepare the evnironment.

- After the environment is ready, build ota provider using following command(output directory is out/debug):

      scripts/examples/gn_build_example.sh examples/ota-provider-app/linux out/debug chip_config_network_layer_ble=false

## Running the OTA Download scenario

- In a Raspberry Pi open a terminal 1 and start the Matter OTA Provider app passing the to it the path to the Matter OTA file created in the previous step

      rm -r /tmp/chip_*
        
      ./chip-ota-provider-app -f RT584HA4-lighting-app-example.ota --discriminator 3841

- In a Raspberry Pi terminal 2 run the chip-tool commands to provision the Provider

      ./chip-tool pairing onnetwork-long 2 20202021 3841

      ./chip-tool accesscontrol write acl '[{"fabricIndex": 1, "privilege": 5, "authMode": 2, "subjects": [112233], "targets": null}, {"fabricIndex": 1, "privilege": 3, "authMode": 2, "subjects": null, "targets": null}]' 2 0

- If the RT58x had been previously commissioned, please hold Button SW2 for
    6 seconds to factory-reset the device
    
- In a Raspberry Pi open a terminal 2 and enter following command with operationalDataset where is obtained from the OpenThread Border Router

      ./chip-tool pairing ble-thread 1 hex:<operationalDataset> 20202021 3840

- Once the commissioning process completes enter

      ./chip-tool otasoftwareupdaterequestor announce-ota-provider 2 0 0 0 1 0

- The application device will connect to the Provider and start the image
    download. Once the image is downloaded the device will reboot into the
    downloaded image.

## Managing the Software Version

In order for the Provider to successfully serve the image to a device during the
OTA Software Update process the Software Version parameter that the .ota file
was built with must be greater than the
`CHIP_DEVICE_CONFIG_DEVICE_SOFTWARE_VERSION` parameter set in the application's
`CHIPProjectConfig.h` file. The Software Version parameter is set by the `-vn`
parameter passed to the `rafael_ota_tool.py create` command. For example, if the
application's running image was built with
`CHIP_DEVICE_CONFIG_DEVICE_SOFTWARE_VERSION` set to 1 and if the `.ota` file is
built with `-vn 2` then the Provider will serve the update image when requested.

In order for the OTA Software Update subsystem to consider an update to be
successful and for the NotifyUpdateApplied command to be transmitted the
`CHIP_DEVICE_CONFIG_DEVICE_SOFTWARE_VERSION` in the updated image must exceed the
software version of the running image (continuing the above example, the image
for the update must be built with `CHIP_DEVICE_CONFIG_DEVICE_SOFTWARE_VERSION` set
to 2).
