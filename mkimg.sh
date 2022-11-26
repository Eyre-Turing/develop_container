#!/bin/bash

self_path=$(dirname "$(realpath "$0")")

image_path=${1:-$(mktemp ./img.XXXXXX)}
image_size=${2:-100}	# 大小，单位为M

dd if=/dev/zero "of=${image_path}" bs=1M "count=${image_size}" &&
mkfs.xfs "$image_path" &&
tmpdir=$(mktemp -d) &&
mount "$image_path" "$tmpdir" &&
mkdir -p "$tmpdir/bin" &&
cp "$self_path/busybox-x86_64" "$tmpdir/bin/" &&
cd "$tmpdir/bin/" &&
chmod +x ./busybox-x86_64 &&
./busybox-x86_64 --list | while read i; do
	ln -s ./busybox-x86_64 $i
done &&
cd - &&
umount "$tmpdir" &&
rm -rf "$tmpdir" &&
echo "succeed" &&
exit 0

echo "failed"
exit 1
