SHELL_FOLDER=$(cd "$(dirname "$0")";pwd)

mkdir build
cd build
../configure --prefix=$SHELL_FOLDER/output/qemu  --target-list=riscv64-softmmu --enable-gtk  --enable-virtfs --disable-gio
make -j
make install
cd ..