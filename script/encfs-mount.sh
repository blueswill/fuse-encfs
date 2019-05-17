#! /bin/bash

CONFIG=
DEVICE=
PKEY=
MOUNT_DIR=/tmp/mnt

if ! [[ -d $MOUNT_DIR ]]; then
    mkdir -p $MOUNT_DIR
fi

function log() {
    logger -st fuse-encfs "$*"
}

function json_get() {
    local NAME=$1
    echo $(eval echo $(jq ".$NAME" $CONFIG))
}

while getopts c:d:p: OPTS; do
    case $OPTS in
        c) CONFIG=$OPTARG
            ;;
        d) DEVICE=/dev/$OPTARG
            ;;
        p) PATH="$OPTARG:$PATH"
            ;;
        *) log "unknown options"
            ;;
    esac
done

if [[ -z $CONFIG || -z $DEVICE ]]; then
    log "missing config or device specified"
    exit 1
fi

if encfs-check --config=$CONFIG --device=$DEVICE; then
    encfs-mount --block-device=$DEVICE \
        --pkey=$(json_get pkey) \
        --target=$DEVICE \
        --owner=$(json_get owner) \
        --primary=$(json_get primary) \
        --private=$(json_get private) \
        --public=$(json_get public) \
        --object=$(json_get object) \
        "$MOUNT_DIR" &&
        losetup -f "$MOUNT_DIR/target"
    exit $?
fi

exit 1
