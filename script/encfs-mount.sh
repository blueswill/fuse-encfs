#! /bin/bash

CONFIG=
DEVICE=
PKEY=
# environment variable
# MOUNT_DIR

if [[ -z $MOUNT_DIR ]]; then
    export MOUNT_DIR=/tmp
fi

export MOUNT_DIR=$(mktemp "$MOUNT_DIR/XXXXXX")

function log() {
    logger -st fuse-encfs "$*"
}

function json_get() {
    local NAME=$1
    JSON_VALUE=$(eval echo $(jq ".$NAME" $CONFIG))
}

while getopts c:d:p: OPTS; do
    case $OPTS in
        c) config=$OPTARG
            ;;
        d) device=$OPTARG
            ;;
        p) PATH="$OPTARG:$PATH"
            ;;
        *) log "unknown options"
            ;;
    esac
done

if [[ -z $CONFIG  -o -z $DEVICE ]]; then
    log "missing config or device specified"
    exit 1
fi

if encfs-check --config=$CONFIG --device=$DEVICE; then
    encfs-mount --block-device=$DEVICE \
        --pkey=$(json_get pkey) \
        --target=$DEVICE \
        --owner$(json_get owner) \
        --primary=$(json_get primary) \
        --private=$(json_get private) \
        --public=$(json_get public) \
        --object=$(json_get object) \
        "$MOUNT_DIR" &&
        losetup -f "$MOUNT_DIR/target"
    exit $?
fi

exit 1
