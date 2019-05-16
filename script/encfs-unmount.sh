#! /bin/bash

function log() {
    logger -st fuse-encfs "$*"
}

# remove loop device
LOOPS=$(losetup -j "$MOUNT_DIR/target")

if [[ -n $LOOPS ]]; then
    if ! losetup -d $(echo $LOOPS|grep -Po "/dev/loop[0-9]*"); then
        exit 1
    fi
fi

unmount $MOUNT_DIR
