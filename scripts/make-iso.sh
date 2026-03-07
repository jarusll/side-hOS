#!/bin/bash

CLONE_DIR=$HOME/source/clones
BUILD_DIR=build
INPUT=$BUILD_DIR/kernel.elf
OUTPUT=$BUILD_DIR/side-hOS.iso
ISO_DIR=iso_root

# Download the latest Limine binary release for the 10.x branch.
git clone https://codeberg.org/Limine/Limine.git $CLONE_DIR/limine --branch=v10.x-binary --depth=1

# Build "limine" utility.
make -C $CLONE_DIR/limine

# Create a directory which will be our ISO root.
mkdir -p $ISO_DIR

# Copy the relevant files over.
mkdir -p $ISO_DIR/boot/limine
cp -v $INPUT $ISO_DIR/boot/
cp -v limine.conf $CLONE_DIR/limine/limine-bios.sys $CLONE_DIR/limine/limine-bios-cd.bin \
    $CLONE_DIR/limine/limine-uefi-cd.bin $ISO_DIR/boot/limine/

# Create the EFI boot tree and copy Limine's EFI executables over.
mkdir -p $ISO_DIR/EFI/BOOT
cp -v $CLONE_DIR/limine/BOOTX64.EFI $ISO_DIR/EFI/BOOT/
cp -v $CLONE_DIR/limine/BOOTIA32.EFI $ISO_DIR/EFI/BOOT/

# Create the bootable ISO.
xorriso -as mkisofs -R -r -J -b boot/limine/limine-bios-cd.bin \
        -no-emul-boot -boot-load-size 4 -boot-info-table -hfsplus \
        -apm-block-size 2048 --efi-boot boot/limine/limine-uefi-cd.bin \
        -efi-boot-part --efi-boot-image --protective-msdos-label \
        $ISO_DIR -o $OUTPUT

# Install Limine stage 1 and 2 for legacy BIOS boot.
limine bios-install $OUTPUT

# cleanup
rm -rf $ISO_DIR
