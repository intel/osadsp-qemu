if [ $# -lt 1 ]
then
  echo "usage: $0 device"
  echo "supported devices: byt, cht"
  exit
fi

case $1 in
*byt)
 CPU="baytrail"
 ADSP="adsp_byt"
  ;;
*cht)
 CPU="baytrail"
 ADSP="adsp_cht"
  ;;
*)
  echo "usage: $0 device"
  echo "supported devices: byt, cht"
  ./xtensa-softmmu/qemu-system-xtensa -machine help
  exit
  ;;
esac


# clear old queues and memory
rm -fr /dev/shm/qemu-bridge*
rm -fr /dev/mqueue/qemu-io-*

#
# Using GDB
#
# 1) Start Qemu VM with a GDB option from below.
#
# 2) Start GDB and point it to reef ELF binary
#
#    xtensa-hifi2ep-elf-gdb reef/src/arch/xtensa/reef
#
# 3) Connect gdb to remote Qemu target
#
#    (gdb) target remote localhost:1234
#
# 4) Target firmware can now be debugged with regular GDB commands. e.g. :-
#
#    Reading symbols from reef/src/arch/xtensa/reef...done.
#    (gdb) target remote localhost:1234
#    Remote debugging using localhost:1234
#    wait_for_interrupt () at wait.S:23
#    23		abi_return
#    (gdb) c
#    Continuing.
#    ^C
#    Program received signal SIGINT, Interrupt.
#    wait_for_interrupt () at wait.S:23
#    23		abi_return
#    (gdb) bt
#    #0  wait_for_interrupt () at wait.S:23
#    #1  0xff2c0f51 in do_task (argc=argc@entry=-4, argv=argv@entry=0xe0000000) at audio.c:44
#    #2  0xff2c0b30 in main (argc=-4, argv=0xe0000000) at init.c:51
#    (gdb)
#

# start the DSP virtualization - GDB disabled
./xtensa-softmmu/qemu-system-xtensa -cpu $CPU -M $ADSP -nographic -S
#./xtensa-softmmu/qemu-system-xtensa -cpu hifi2_ep -M adsp_byt -nographic -d guest_errors,mmu,int,in_asm -kernel ../reef/src/arch/xtensa/reef.bin
#./xtensa-softmmu/qemu-system-xtensa -cpu hifi2_ep -M adsp_byt -nographic -d guest_errors,mmu -kernel ~/source/reef/image.bin
#./xtensa-softmmu/qemu-system-xtensa -cpu hifi2_ep -M adsp_byt -nographic -kernel ../reef/src/arch/xtensa/reef.bin

# start the DSP virtualization - GDB enabled with CPU running
#./xtensa-softmmu/qemu-system-xtensa -s -cpu hifi2_ep -M adsp_byt -nographic

# start the DSP virtualization - GDB enabled with CPU stalled
#./xtensa-softmmu/qemu-system-xtensa -s -S -cpu hifi2_ep -M adsp_byt -nographic

# VM can execute flat binary files at startup using -kernel cmd line option
# -kernel ../reef/src/arch/xtensa/reef.bin
