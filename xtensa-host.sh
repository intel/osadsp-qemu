if [ $# -lt 1 ]
then
  echo "usage: $0 device [-k kernel] [-t] [-d] [-i] [-r rom] [-c] [-g]"
  echo "supported devices: byt, cht, hsw, bdw, bxt"
  echo "[-k] | [--kernel]: load firmware kernel image"
  echo "[-r] | [--rom]: load firmware ROM image"
  echo "[-t] | [--trace]: trace DSP instructions"
  echo "[-i] | [--irqs]: trace DSP IRQs"
  echo "[-d] | [--debug]: enable GDB debugging - uses tcp::1234"
  echo "[-c] | [--console]: Stall DSP and enter console before executing"
  echo "[-g] | [--guest]: Display guest errors"
  exit
fi

# get device
case $1 in
*byt)
 CPU="baytrail"
 ADSP="adsp_byt"
  ;;
*cht)
 CPU="baytrail"
 ADSP="adsp_cht"
  ;;
*hsw)
 CPU="haswell"
 ADSP="adsp_hsw"
  ;;
*bdw)
 CPU="haswell"
 ADSP="adsp_bdw"
  ;;
*bxt)
 CPU="broxton"
 ADSP="adsp_bxt"
  ;;

*)
  echo "error: unsupported device"
  echo "supported devices: byt, cht, hsw, bdw, bxt"
  ./xtensa-softmmu/qemu-system-xtensa -machine help
  exit
  ;;
esac

# Parse remaining args
ARG=()
while [[ $# -gt 0 ]]
do
key="$2"

case $key in
    -k|--kernel)
    KERNEL="-kernel $3"
    shift # past argument
    shift # past value
    ;;
    -r|--rom)
    ROM="-rom $3"
    shift # past argument
    shift # past value
    ;;
    -t|--trace)
    TARGS="-d mmu,in_asm"
    shift # past argument
    ;;
    -i|--irqs)
    IARGS="-d int"
    shift # past argument
    ;;
    -g|--guest)
    GARGS="-d guest_errors"
    shift # past argument
    ;;
    -d|--debug)
    DARGS="-s -S"
    shift # past argument
    ;;
    -c|--console)
    CARGS="-S"
    shift # past argument
    ;;
    *)    # unknown option
    ARG+=("$1") # save it in an array for later
    shift # past argument
    ;;
esac
done
set -- "${ARG[@]}" # restore arg parameters

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

echo ./xtensa-softmmu/qemu-system-xtensa -cpu $CPU -M $ADSP $TARGS $DARGS $IARGS -nographic $KERNEL $ROM $CARGS $GARGS
./xtensa-softmmu/qemu-system-xtensa -cpu $CPU -M $ADSP $TARGS $DARGS $IARGS -nographic $KERNEL $ROM $CARGS $GARGS

