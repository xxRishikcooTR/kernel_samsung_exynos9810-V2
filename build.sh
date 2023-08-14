PATH="/home/rishik/.clang/bin:${PATH}" \
make -j6 O=out \
 	ARCH=arm64 \
	CC=clang \
 	LD=ld.lld \
 	LLVM=1 \
 	LLVM_IAS=1 \
 	CLANG_TRIPLE=aarch64-linux-gnu- \
	CROSS_COMPILE=aarch64-linux-gnu- \
	CROSS_COMPILE_ARM32=arm-linux-gnueabi- $@