#!/bin/bash

if [ $# -ne 1 ] && [ $# -ne 2 ]; then
	cat >&2 << EOF
must spec create image name!
Usage:
  $0 <image_name> [image_size(unit: M, default 200)]
EOF
	echo "failed"
	exit 1
fi

self_path=$(dirname "$(realpath "$0")")

image_path=$1
image_size=${2:-200}	# 大小，单位为M

self_pwd=$(realpath "$(pwd)")

get_deps()
{
	local bin_file=$1
	ldd "$bin_file" | sed -r 's/.*=>//g' | sed -r 's/^[\ \t]+//g' | sed -r 's/[\ ]?\(.*//g'
}

copy_deps()
{
	local tmpdir=$1
	local bin_file=$2
	local dep
	cd /
	get_deps "$bin_file" | while read dep; do
		[ -z "$dep" ] && continue
		\cp --parent "$dep" "$tmpdir"
	done
	return 0
}

copy_tools()
{
	local tmpdir=$1

	# bash
	local bash_path=$(which bash)
	\cp "$bash_path" "$tmpdir/usr/bin/"
	copy_deps "$tmpdir" "$bash_path" || return 1

	# python
	local python_path=$(realpath "$(which python)")
	\cp "$python_path" "$tmpdir/usr/bin/"
	ln -s "./$(basename "$python_path")" "$tmpdir/usr/bin/python"
	copy_deps "$tmpdir" "$python_path" || return 1
	cp -r "/usr/lib64/$(basename "$python_path")" "$tmpdir/usr/lib64/"
	cat >"$tmpdir/root/.bashrc" << EOF
export PYTHONHOME=/usr/bin
export PYTHONPATH=/usr/bin/$(basename "$python_path"):/usr/lib64/$(basename "$python_path")

PS1='[\\u@\\h \\W]\\\$ '
cd /root
EOF

	# yum
	local yum_path=$(which yum)
	\cp "$yum_path" "$tmpdir/usr/bin/"

	# nc
	local nc_path=$(which nc)
	rm -f "$tmpdir/usr/bin/nc"
	\cp "$nc_path" "$tmpdir/usr/bin/"
	copy_deps "$tmpdir" "$nc_path" || return 1

	return 0
}

dd if=/dev/zero "of=${image_path}" bs=1M "count=${image_size}" &&
mkfs.xfs "$image_path" &&
tmpdir=$(mktemp -d) &&
mount "$image_path" "$tmpdir" &&
mkdir -p "$tmpdir/usr/bin" &&
mkdir -p "$tmpdir/usr/lib64" &&
ln -s "usr/bin" "$tmpdir/bin" &&
ln -s "usr/lib64" "$tmpdir/lib64" &&
mkdir -p "$tmpdir/root" &&
cp "$self_path/busybox-x86_64" "$tmpdir/usr/bin/" &&
cd "$tmpdir/usr/bin/" &&
chmod +x ./busybox-x86_64 &&
./busybox-x86_64 --list | while read i; do
	ln -s ./busybox-x86_64 $i
done &&
copy_tools "$tmpdir" &&
cd "$self_pwd" &&
umount "$tmpdir" &&
rm -rf "$tmpdir" &&
echo "succeed" &&
exit 0

echo "failed"
exit 1
