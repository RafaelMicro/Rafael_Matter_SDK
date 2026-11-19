#!/usr/bin/env bash

#
#    Copyright (c) 2020 Project CHIP Authors
#
#    Licensed under the Apache License, Version 2.0 (the "License");
#    you may not use this file except in compliance with the License.
#    You may obtain a copy of the License at
#
#        http://www.apache.org/licenses/LICENSE-2.0
#
#    Unless required by applicable law or agreed to in writing, software
#    distributed under the License is distributed on an "AS IS" BASIS,
#    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#    See the License for the specific language governing permissions and
#    limitations under the License.
#

# Build script for GN RT58x examples GitHub workflow.

set -e

if [[ -z "${MATTER_ROOT}" ]]; then
    echo "Using default path for Matter root"
    CHIP_ROOT="$(dirname "$0")/../.."
else
    echo "Using ENV path for Matter root"
    CHIP_ROOT="$MATTER_ROOT"
fi

source "$CHIP_ROOT/scripts/activate.sh"

set -x
env
USE_WIFI=false

USAGE="./scripts/examples/gn_rt58x_example.sh <AppRootFolder> <outputFolder> <rafael_board_name> [<Build options>]"

if [ "$#" == "0" ]; then
    echo "Build script for RT58x Matter apps
    Format:
    $USAGE

    <AppRootFolder>
        Root Location of the app e.g: examples/lighting-app/rt58x/

    <outputFolder>
        Desired location for the output files

    <rafael_board_name>
        Identifier of the board for which this app is built
        Currently Supported :
            RT583

    <Build options> - optional noteworthy build options for RT58x
        --sed
            enable sleepy end device (SED), set thread mtd
        --ssed
            enable synchronized sleepy end device (SSED)
        (neither: non-sleepy device (default), thread ftd)
        --certs
            yes: using rafael dac/pai/cd
            dac-only: using rafael dac/pai and example cd
            no: using example dac/pai/cd
    "
elif [ "$#" -lt "2" ]; then
    echo "Invalid number of arguments
    Format:
    $USAGE"
else
    ROOT=$1
    OUTDIR=$2

    if [ "$#" -gt "2" ]; then
        RAFAEL_BOARD=$3
        shift
    fi

    shift
    shift
    while [ $# -gt 0 ]; do
        case $1 in
        --sed)
            optArgs+="enable_sleepy_device=true chip_enable_icd_server=true chip_openthread_ftd=false chip_build_libshell=false "
            shift
            ;;

        --ssed)
            # SSED = SED + openthread CSL receiver (RAF_ENABLE_SSED in OpenThreadConfig.h)
            optArgs+="enable_sleepy_device=true chip_enable_icd_server=true chip_openthread_ftd=false chip_build_libshell=false raf_enable_ssed=true "
            shift
            ;;

        --certs=yes)
            optArgs+="chip_build_platform_attestation_credentials_provider=true "
            # optArgs+="chip_build_platform_attestation_credentials_provider=true chip_use_transitional_commissionable_data_provider=false "
            shift
            ;;

        --certs=dac-only)
            optArgs+="chip_build_platform_attestation_credentials_provider=true chip_using_default_cd=true"
            # optArgs+="chip_build_platform_attestation_credentials_provider=true chip_use_transitional_commissionable_data_provider=false "
            shift
            ;;
        *)
            shift
            ;;
        esac
    done

    if [ "$RAFAEL_BOARD" != "RT583" ] && [ "$RAFAEL_BOARD" != "RT584L" ] && [ "$RAFAEL_BOARD" != "RT584H" ] && [ "$RAFAEL_BOARD" != "RT584HA4" ]; then
        echo "RAFAEL_BOARD is not defined"
        exit 1
    fi

    BUILD_DIR=$OUTDIR/$RAFAEL_BOARD
    echo BUILD_DIR="$BUILD_DIR"

    # thread build
    #
    BUILD_START=$SECONDS
    if [ -z "$optArgs" ]; then
        gn gen --check --fail-on-unused-args --export-compile-commands --root="$ROOT" --args="rafael_board=\"$RAFAEL_BOARD\" treat_warnings_as_errors=false" "$BUILD_DIR"
    else
        gn gen --check --fail-on-unused-args --export-compile-commands --root="$ROOT" --args="rafael_board=\"$RAFAEL_BOARD\" $optArgs treat_warnings_as_errors=false" "$BUILD_DIR"
    fi
    ninja -C "$BUILD_DIR"/
    # ninja -v -C "$BUILD_DIR"/
    BUILD_END=$SECONDS
    # print stats
    arm-none-eabi-size -A "$BUILD_DIR"/*.out
    arm-none-eabi-size -A "$BUILD_DIR"/*.out > "$BUILD_DIR"/size.txt

    { set +x; } 2>/dev/null
    TOTAL_ELAPSED=$((BUILD_END - BUILD_START))
    printf "Build elapsed time: %dm%02ds\n" \
        $((TOTAL_ELAPSED / 60)) $((TOTAL_ELAPSED % 60))
fi
