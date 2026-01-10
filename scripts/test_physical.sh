#!/bin/bash
#
# test_physical.sh - Test MyOS on physical hardware checklist
#
# This script provides a checklist and commands for testing MyOS
# on physical hardware

echo "============================================"
echo "  MyOS Physical Hardware Test Guide"
echo "============================================"
echo ""

echo "== Prerequisites =="
echo "  1. Build MyOS: make clean && make"
echo "  2. Create bootable USB: sudo ./scripts/make_usb.sh /dev/sdX"
echo "  3. (Optional) Convert to hybrid ISO: ./scripts/make_hybrid_iso.sh"
echo ""

echo "== BIOS Boot Test =="
echo "  1. Insert USB into target machine"
echo "  2. Enter BIOS setup (F2/Del during boot)"
echo "  3. Set USB as first boot device"
echo "  4. Disable Secure Boot if present"
echo "  5. Save and reboot"
echo ""

echo "== Expected Boot Output =="
cat << 'EOF'
  =============================
    Hello from MyOS!
    64-bit kernel running
  =============================

  [PMM] Physical memory manager initialized
  [HEAP] Kernel heap initialized
  [VMM] Virtual memory manager initialized
  [PIT] Programmable Interval Timer initialized
  [PCI] Scanning PCI bus...
  [AHCI] Found controller: xxxx:xxxx at xx:xx.x
  [AHCI] Port X: <disk model> (XXXX MB)
  [ACPI] RSDP found, OEM: <vendor>
  [ACPI] Initialization complete
  ...
  MyOS Shell v0.2
  / # _
EOF
echo ""

echo "== Troubleshooting =="
echo "  Q: System doesn't boot from USB"
echo "  A: - Check BIOS boot order"
echo "     - Try different USB port (USB 2.0 vs 3.0)"
echo "     - Disable Fast Boot in BIOS"
echo "     - Try CSM/Legacy mode instead of UEFI"
echo ""
echo "  Q: No video output"
echo "  A: - Connect serial cable to COM1 (115200 8N1)"
echo "     - Check if system has VGA-compatible graphics"
echo ""
echo "  Q: Disk not detected"
echo "  A: - Check SATA mode in BIOS (AHCI vs IDE)"
echo "     - Try different SATA port"
echo "     - Some older controllers may not be supported"
echo ""
echo "  Q: Kernel panic"
echo "  A: - Connect serial console for debug output"
echo "     - Report issue with CPU model and BIOS version"
echo ""

echo "== Serial Console =="
echo "  For debugging, connect a serial cable and run:"
echo "    screen /dev/ttyUSB0 115200"
echo "  or"
echo "    minicom -D /dev/ttyUSB0 -b 115200"
echo ""

echo "== QEMU Test with AHCI =="
echo "  Test AHCI driver in QEMU:"
echo "    qemu-system-x86_64 \\"
echo "      -cdrom myos.iso \\"
echo "      -drive file=test.img,format=raw,if=none,id=disk0 \\"
echo "      -device ahci,id=ahci \\"
echo "      -device ide-hd,drive=disk0,bus=ahci.0 \\"
echo "      -nographic"
echo ""

echo "== Hardware Compatibility =="
echo "  Tested on:"
echo "    - QEMU/KVM with virtio and IDE"
echo "    - VirtualBox"
echo "    - VMware Workstation"
echo "  "
echo "  Should work on:"
echo "    - Intel/AMD x86_64 systems with BIOS/UEFI"
echo "    - SATA disks via AHCI controller"
echo "    - PS/2 or USB keyboards"
echo ""
echo "  Not yet supported:"
echo "    - NVMe SSDs"
echo "    - eMMC storage"
echo "    - Newer USB 3.1/3.2 only ports"
echo ""

echo "== Reporting Issues =="
echo "  If you encounter problems on physical hardware,"
echo "  please report at: https://github.com/your/myos/issues"
echo "  Include:"
echo "    - CPU model (cat /proc/cpuinfo)"
echo "    - Motherboard model"
echo "    - BIOS/UEFI version"
echo "    - Serial console output (if available)"
echo ""
