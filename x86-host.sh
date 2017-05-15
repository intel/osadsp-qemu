if [ $# -lt 1 ]
then
  echo "usage: $0 device"
  echo "supported devices: byt, cht, hsw, bdw, bxt"
  exit
fi

case $1 in
*byt)
 CPU="baytrail"
 ADSP="adsp-byt"
  ;;
*cht)
 CPU="baytrail"
 ADSP="adsp-cht"
  ;;
*hsw)
 CPU="haswell"
 ADSP="adsp-hsw"
  ;;
*bdw)
 CPU="haswell"
 ADSP="adsp-bdw"
  ;;
*bxt)
 CPU="broxton"
 ADSP="adsp-bxt"
  ;;
*)
  echo "usage: $0 device"
  echo "supported devices: byt, cht, hsw, bdw, bxt"
  ./xtensa-softmmu/qemu-system-xtensa -machine help
  exit
  ;;
esac

# start the x86 host use -gdb flag to enable GDB
./x86_64-softmmu/qemu-system-x86_64 -enable-kvm -name ubuntu-intel  -machine pc,accel=kvm,usb=off -cpu SandyBridge -m 2048 -realtime mlock=off -smp 2,sockets=2,cores=1,threads=1 -uuid f5e20908-1854-41b5-b2ca-81b2f007d92d -no-user-config -nodefaults -rtc base=utc,driftfix=slew -global kvm-pit.lost_tick_policy=discard -no-hpet -no-shutdown -global PIIX4_PM.disable_s3=1 -global PIIX4_PM.disable_s4=1 -boot strict=on -device virtio-serial-pci,id=virtio-serial0,bus=pci.0,addr=0x6 -chardev pty,id=charserial0 -device isa-serial,chardev=charserial0,id=serial0 -chardev spicevmc,id=charchannel0,name=vdagent -device virtserialport,bus=virtio-serial0.0,nr=1,chardev=charchannel0,id=channel0,name=com.redhat.spice.0 -spice port=5901,addr=127.0.0.1,disable-ticketing,seamless-migration=on -device bxt-intel-hda,id=sound0,bus=pci.0,addr=0xe -device qxl-vga,id=video0,ram_size=67108864,vram_size=67108864,vgamem_mb=16,bus=pci.0,addr=0x2 -device virtio-balloon-pci,id=balloon0,bus=pci.0,addr=0x8 -msg timestamp=on -display sdl -netdev user,id=network0 -device e1000,netdev=network0 -redir tcp:5555::22 -monitor stdio -device driver=$ADSP ~/source/intel/Reef/ubuntu-xtensa-dev.qcow2

# ubuntu cmd line for qemu
#qemu-system-x86_64 -enable-kvm -name guest=generic,debug-threads=on -S -object secret,id=masterKey0,format=raw,file=/var/lib/libvirt/qemu/domain-1-generic/master-key.aes -machine pc-i440fx-yakkety,accel=kvm,usb=off,dump-guest-core=off -cpu Skylake-Client -m 4096 -realtime mlock=off -smp 2,sockets=2,cores=1,threads=1 -uuid 09bb5631-5594-4122-8040-f3b833231021 -no-user-config -nodefaults -chardev socket,id=charmonitor,path=/var/lib/libvirt/qemu/domain-1-generic/monitor.sock,server,nowait -mon chardev=charmonitor,id=monitor,mode=control -rtc base=utc,driftfix=slew -global kvm-pit.lost_tick_policy=discard -no-hpet -no-shutdown -global PIIX4_PM.disable_s3=1 -global PIIX4_PM.disable_s4=1 -boot strict=on -device ich9-usb-ehci1,id=usb,bus=pci.0,addr=0x6.0x7 -device ich9-usb-uhci1,masterbus=usb.0,firstport=0,bus=pci.0,multifunction=on,addr=0x6 -device ich9-usb-uhci2,masterbus=usb.0,firstport=2,bus=pci.0,addr=0x6.0x1 -device ich9-usb-uhci3,masterbus=usb.0,firstport=4,bus=pci.0,addr=0x6.0x2 -device virtio-serial-pci,id=virtio-serial0,bus=pci.0,addr=0x5 -drive file=/media/lrg/ff57a4a5-de44-4527-a405-8cd67b00975b/lrg/vm/Ubuntu-Intel-20G.img,format=raw,if=none,id=drive-ide0-0-0 -device ide-hd,bus=ide.0,unit=0,drive=drive-ide0-0-0,id=ide0-0-0,bootindex=1 -netdev tap,fd=26,id=hostnet0 -device rtl8139,netdev=hostnet0,id=net0,mac=52:54:00:7e:e3:44,bus=pci.0,addr=0x3 -chardev pty,id=charserial0 -device isa-serial,chardev=charserial0,id=serial0 -chardev spicevmc,id=charchannel0,name=vdagent -device virtserialport,bus=virtio-serial0.0,nr=1,chardev=charchannel0,id=channel0,name=com.redhat.spice.0 -spice port=5900,addr=127.0.0.1,disable-ticketing,image-compression=off,seamless-migration=on -device qxl-vga,id=video0,ram_size=67108864,vram_size=67108864,vram64_size_mb=0,vgamem_mb=16,max_outputs=1,bus=pci.0,addr=0x2 -device intel-hda,id=sound0,bus=pci.0,addr=0x4 -device hda-duplex,id=sound0-codec0,bus=sound0.0,cad=0 -chardev spicevmc,id=charredir0,name=usbredir -device usb-redir,chardev=charredir0,id=redir0,bus=usb.0,port=1 -chardev spicevmc,id=charredir1,name=usbredir -device usb-redir,chardev=charredir1,id=redir1,bus=usb.0,port=2 -device virtio-balloon-pci,id=balloon0,bus=pci.0,addr=0x7 -msg timestamp=on
