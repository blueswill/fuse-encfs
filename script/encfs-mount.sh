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

function device_mount() {
    return $?
}

function get_id_list() {
    XIFS=$IFS
    XGLOBIGNORE=$GLOBIGNORE
    IFS=$'\n'
    GLOBIGNORE='*'
    local ID_FILE=$(json_get default_id_file)
    local ID_FILE_ARRAY=($(<${ID_FILE}))
    local tmp="$(printf ",%s" "${ID_FILE_ARRAY[@]}")"
    echo "${tmp:1}"
    IFS=$XIFS
    GLOBIGNORE=$XGLOBIGNORE
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

if ! encfs-check --config=$CONFIG --device=$DEVICE; then
    if [[ $(json_get create_flag) == "true" ]]; then
        logger "creating device $DEVICE"
        encfs-create -m $(json_get master_key) \
            -b $DEVICE \
            -t $(get_id_list) \
            -d $(json_get private_key_dir) \
            -o $(json_get owner) \
            -p $(json_get primary) \
            -r $(json_get private) \
            -u $(json_get public) \
            -s $(json_get object)

        [[ $? -eq 0 ]] || exit $?
    else
        exit 1
    fi
fi

logger "mounting device $DEVICE"
encfs-mount --block-device=$DEVICE \
    --pkey=$(json_get pkey) \
    --target=$DEVICE \
    --owner=$(json_get owner) \
    --primary=$(json_get primary) \
    --private=$(json_get private) \
    --public=$(json_get public) \
    --object=$(json_get object) \
    "$MOUNT_DIR" && losetup -f "$MOUNT_DIR/target"

exit $?
