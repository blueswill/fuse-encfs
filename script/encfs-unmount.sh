#! /bin/bash

MOUNT_DIR=/tmp/mnt

function log() {
    logger -st fuse-encfs "$*"
}

# remove loop device
LOOPS=$(losetup -j "$MOUNT_DIR/target")

if [[ -n $LOOPS ]]; then
    LOOP=$(echo $LOOPS|grep -Po "/dev/loop[0-9]*")
    log $(printf "remove loop device %s" $LOOP)
    if ! losetup -d $LOOP; then
        exit 1
    fi
fi

umount $MOUNT_DIR
