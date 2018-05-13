if [ $# -lt 1 ]
then
  echo "usage: $0 device fs_image"
  echo "supported devices: byt, cht, bsw, hsw, bdw, bxt, apl"
  exit
fi

case $1 in
*byt)
 CPU="baytrail"
 ADSP="-device driver=adsp-byt"
 HDA=""
  ;;
*cht)
 CPU="baytrail"
 ADSP="-device driver=adsp-cht"
 HDA=""
  ;;
*bsw)
 CPU="baytrail"
 ADSP="-device driver=adsp-cht"
 HDA=""
  ;;
*hsw)
 CPU="haswell"
 ADSP="-device driver=adsp-hsw"
 HDA=""
  ;;
*bdw)
 CPU="haswell"
 ADSP="-device driver=adsp-bdw"
 HDA=""
  ;;
*bxt)
 CPU="broxton"
 ADSP="-device driver=adsp-bxt"
 HDA=" -device bxt-intel-hda,id=sound0,bus=pci.0,addr=0xe"
  ;;
*apl)
 CPU="broxton"
 ADSP="-device driver=adsp-bxt"
 HDA=" -device bxt-intel-hda,id=sound0,bus=pci.0,addr=0xe"
  ;;
*)
  echo "usage: $0 device"
  echo "supported devices: byt, cht, bsw, hsw, bdw, bxt, apl"
  ./xtensa-softmmu/qemu-system-xtensa -machine help
  exit
  ;;
esac

# start the x86 host use -gdb flag to enable GDB
./x86_64-softmmu/qemu-system-x86_64 -enable-kvm -name ubuntu-intel  \
	-machine pc,accel=kvm,usb=off \
	-cpu SandyBridge -m 2048 \
	-realtime mlock=off \
	-smp 2,sockets=2,cores=1,threads=1 \
	-uuid f5e20908-1854-41b5-b2ca-81b2f007d92d \
	-no-user-config -nodefaults \
	-rtc base=utc,driftfix=slew \
	-global kvm-pit.lost_tick_policy=discard -no-hpet \
	-no-shutdown \
	-global PIIX4_PM.disable_s3=1 \
	-global PIIX4_PM.disable_s4=1 \
	-boot strict=on \
	-device virtio-serial-pci,id=virtio-serial0,bus=pci.0,addr=0x6 \
	-chardev pty,id=charserial0 \
	-device isa-serial,chardev=charserial0,id=serial0 \
	-chardev spicevmc,id=charchannel0,name=vdagent \
	-device virtserialport,bus=virtio-serial0.0,nr=1,chardev=charchannel0,id=channel0,name=com.redhat.spice.0 \
	-spice port=5902,addr=127.0.0.1,disable-ticketing,seamless-migration=on \
	-device qxl-vga,id=video0,ram_size=67108864,vram_size=67108864,vgamem_mb=16,bus=pci.0,addr=0x2 \
	-device virtio-balloon-pci,id=balloon0,bus=pci.0,addr=0x8 \
	-msg timestamp=on \
	-display sdl \
	-netdev user,id=network0 -device e1000,netdev=network0 \
	-redir tcp:5555::22 \
	-monitor stdio $HDA $ADSP $2

