dtb-$(CONFIG_ARCH_HISI) += hi6220-hikey.dtb hip05-d02.dtb
dtb-$(CONFIG_ARCH_HI3798CV2X) += hi3798cv200.dtb
dtb-$(CONFIG_ARCH_HI3796MV2X) += hi3796mv200.dtb
dtb-$(CONFIG_ARCH_HISI) += hi3798mv300.dtb

ifdef CONFIG_ARCH_HI3798MV2X
ifdef CONFIG_HI3798MV2X_FPGA
dtb-y += hi3798mv200_fpga.dtb
else
dtb-y += hi3798mv200.dtb
endif
endif

always		:= $(dtb-y)
subdir-y	:= $(dts-dirs)
clean-files	:= *.dtb
