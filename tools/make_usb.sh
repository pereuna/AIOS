#!/usr/bin/env bash
set -Eeuo pipefail

usage() {
    cat >&2 <<'EOF'
Usage: tools/make_usb.sh [--format --yes] [--source DIR] /dev/sdX

Without --format, an existing FAT32 first partition is used.  --format
creates a new GPT/EFI FAT32 partition and requires --yes because it erases
the selected disk.  The source defaults to the AIOS checkout.
EOF
    exit 2
}

format=0
yes=0
source_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
disk=
while (($#)); do
    case $1 in
        --format) format=1 ;;
        --yes) yes=1 ;;
        --source) shift; (($#)) || usage; source_dir=$(CDPATH= cd -- "$1" && pwd) ;;
        -h|--help) usage ;;
        /dev/*) [[ -z "$disk" ]] || usage; disk=$1 ;;
        *) usage ;;
    esac
    shift
done
[[ -n "$disk" ]] || usage
[[ -b "$disk" ]] || { echo "Not a block device: $disk" >&2; exit 1; }
[[ "$(lsblk -dnro TYPE "$disk")" == disk ]] || { echo "Expected a whole disk: $disk" >&2; exit 1; }
[[ "$(lsblk -dnro RM "$disk")" == 1 ]] || { echo "Refusing non-removable disk (RM is not 1): $disk" >&2; exit 1; }

efi="$source_dir/dist/EFI/BOOT/BOOTX64.EFI"
model="$source_dir/dist/model.bin"
[[ -f "$efi" && -f "$model" ]] || { echo "Build dist first: missing EFI image or model.bin in $source_dir/dist" >&2; exit 1; }

part="${disk}1"
[[ "$disk" == /dev/nvme* || "$disk" == /dev/mmcblk* ]] && part="${disk}p1"
mountpoint=
mounted_by_us=0
cleanup() {
    if ((mounted_by_us)); then
        if command -v udisksctl >/dev/null 2>&1; then udisksctl unmount -b "$part" >/dev/null || true
        else umount "$mountpoint" >/dev/null || true
        fi
    fi
    [[ -n "${tmp_mount:-}" ]] && rmdir "$tmp_mount" 2>/dev/null || true
}
trap cleanup EXIT

if ((format)); then
    ((yes)) || { echo "--format erases $disk; repeat with --format --yes" >&2; exit 1; }
    if lsblk -nrpo NAME,MOUNTPOINT "$disk" | awk '$2 != "" && $2 != "-" { found=1 } END { exit found ? 0 : 1 }'; then
        echo "Unmount all partitions on $disk before formatting" >&2; exit 1
    fi
    echo "Formatting removable disk $disk (--yes supplied)" >&2
    command -v sfdisk >/dev/null || { echo "Missing sfdisk" >&2; exit 1; }
    command -v mkfs.vfat >/dev/null || { echo "Missing mkfs.vfat" >&2; exit 1; }
    printf 'label: gpt\n, , U\n' | sfdisk --wipe always "$disk" >/dev/null
    partprobe "$disk" 2>/dev/null || true
    for _ in {1..20}; do [[ -b "$part" ]] && break; sleep 0.25; done
    [[ -b "$part" ]] || { echo "Partition did not appear: $part" >&2; exit 1; }
    mkfs.vfat -F 32 -n AIOS "$part" >/dev/null
else
    [[ -b "$part" ]] || { echo "Missing $part; use --format --yes to create it" >&2; exit 1; }
    [[ "$(lsblk -dnro FSTYPE "$part")" == vfat ]] || { echo "$part is not FAT32 (use --format --yes to recreate it)" >&2; exit 1; }
fi

mountpoint=$(findmnt -rn -S "$part" -o TARGET 2>/dev/null || true)
if [[ -z "$mountpoint" ]]; then
    if command -v udisksctl >/dev/null 2>&1; then
        mountpoint=$(udisksctl mount -b "$part" | sed -n 's/^Mounted .* at //p')
    else
        tmp_mount=$(mktemp -d /tmp/aios-usb.XXXXXX)
        mount "$part" "$tmp_mount"
        mountpoint=$tmp_mount
    fi
    [[ -n "$mountpoint" && -d "$mountpoint" ]] || { echo "Could not mount $part" >&2; exit 1; }
    mounted_by_us=1
fi

same_file() {
    local a=$1 b=$2
    [[ -f "$b" ]] || return 1
    [[ "$(stat -c %s "$a")" == "$(stat -c %s "$b")" ]] || return 1
    [[ "$(sha256sum "$a" | awk '{print $1}')" == "$(sha256sum "$b" | awk '{print $1}')" ]]
}
copy_file() {
    local src=$1 dst=$2
    if same_file "$src" "$dst"; then
        echo "unchanged: ${dst#$mountpoint/}"
        return
    fi
    mkdir -p "$(dirname -- "$dst")"
    cp "$src" "$dst.part"
    sync
    mv -f "$dst.part" "$dst"
    echo "updated: ${dst#$mountpoint/}"
}

copy_file "$efi" "$mountpoint/EFI/BOOT/BOOTX64.EFI"
copy_file "$model" "$mountpoint/model.bin"
sync
echo "USB boot image ready on $part"
