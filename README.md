# magiscan
Linux implementation for Hyundai Magic Scan mini scanner. And a Windows implementation.

## Current state
Scanning under Linux works but is quite slow possibly due to some weirdness in the SCSI generic implementation.

## Linux tool
- plug in your Magic Scan device
- you may need to adapt cDiskName in magiscan.cpp
- <code>make</code> and as root/sudo, do: <code>./magiscan</code>

Example output:
<pre>init ok
duration: 168 ms status: 0x0 host_status 0x7 driver_status 0x0 resid 0
[...]
GetDeviceState: 00000000
DeviceState: 0 => IDLE
duration: 168 ms status: 0x0 host_status 0x7 driver_status 0x0 resid 0
[...]
VendorCmdGetData(code=1)#1: 100000000100000001000000000000005000000050dd2000
duration: 168 ms status: 0x0 host_status 0x7 driver_status 0x0 resid 0
duration: 168 ms status: 0x0 host_status 0x7 driver_status 0x0 resid 0
VendorCmdGetData(code=1)#1: 000000000100000000000000800200005000000000000000
duration: 168 ms status: 0x0 host_status 0x7 driver_status 0x0 resid 0
duration: 168 ms status: 0x0 host_status 0x7 driver_status 0x0 resid 0
DataInfo status:0 (NOT_READY) color:1 dpi:0 width:640 height:80 addr:0
[...]
</pre>
Now hold the Scan button on the device and drag it slowly to scan:
<pre>
DataInfo status:1 (READY) color:1 dpi:0 width:640 height:80 addr:20dd50
lowlevelReadCmd: c3070020dd5000010000000000000000
duration: 168 ms status: 0x0 host_status 0x7 driver_status 0x0 resid 0
read chunk of 0x10000 bytes, start addr 0x20dd50, bufptr 0x608010
lowlevelReadCmd: c3070021dd5000010000000000000000
duration: 168 ms status: 0x0 host_status 0x7 driver_status 0x0 resid 0
read chunk of 0x10000 bytes, start addr 0x21dd50, bufptr 0x618010
lowlevelReadCmd: c3070022dd5000005800000000000000
duration: 168 ms status: 0x0 host_status 0x7 driver_status 0x0 resid 0
read chunk of 0x5800 bytes, start addr 0x22dd50, bufptr 0x628010
read 153600 bytes
</pre>
Here you can see a bunch of image data with 640x80px totalling 153 kB being received.

The image date is being written to <code>out.data</code> and you can view the image by opening it in GIMP as RAW data with 640x80px resolution.

## Windows tool

The Windows tool <code>mscan</code> is located in the <code>mscan</code> directory. It outputs the scanned image chunks as BMP files
and its main use is for reverse engineering.

You can compile the mscan for Windows project with Code::Blocks IDE: http://www.codeblocks.org/

Get the original Magic Scan software from: http://www.my-hyundai.de/magic-scan.html
And install it.

For my Windows implementation to work you need to copy NvUSB.dll from the
Magic Scan Windows driver installation to this directory.

I used this version of NvUSB.dll successfully:
<pre>
% md5sum NvUSB.dll
a7a36feb01a389a571a46f82051349f8  NvUSB.dll
</pre>

## Info about Hyundai Magic Scan
What the kernel says:
<pre>
[ 7869.544087] usb 1-1.4.2: new high-speed USB device number 4 using ehci-pci
[ 7869.637722] usb-storage 1-1.4.2:1.0: USB Mass Storage device detected
[ 7869.638449] scsi host7: usb-storage 1-1.4.2:1.0
[ 7870.636459] scsi 7:0:0:0: Direct-Access              MagicScan         1.0 PQ: 0 ANSI: 5
[ 7870.636692] sd 7:0:0:0: Attached scsi generic sg2 type 0
[ 7870.638084] sd 7:0:0:0: [sdc] Attached SCSI removable disk
</pre>

lsusb Output:
<pre>
Bus 001 Device 005: ID 0603:8611 Novatek Microelectronics Corp.
Device Descriptor:
  bLength                18
  bDescriptorType         1
  bcdUSB               2.00
  bDeviceClass            0 (Defined at Interface level)
  bDeviceSubClass         0                                                                                                                                                                                                                         
  bDeviceProtocol         0                                                                                                                                                                                                                         
  bMaxPacketSize0        64
  idVendor           0x0603 Novatek Microelectronics Corp.
  idProduct          0x8611                                                                                                                                                                                                                         
  bcdDevice            0.01
  iManufacturer           1
  iProduct                2
  iSerial                 3
  bNumConfigurations      1
  Configuration Descriptor:
    bLength                 9
    bDescriptorType         2
    wTotalLength           32
    bNumInterfaces          1
    bConfigurationValue     1
    iConfiguration          0
    bmAttributes         0xc0
      Self Powered                                                                                                                                                                                                                                  
    MaxPower              100mA
    Interface Descriptor:                                                                                                                                                                                                                           
      bLength                 9
      bDescriptorType         4
      bInterfaceNumber        0
      bAlternateSetting       0
      bNumEndpoints           2
      bInterfaceClass         8 Mass Storage                                                                                                                                                                                                        
      bInterfaceSubClass      6 SCSI
      bInterfaceProtocol     80 Bulk-Only
      iInterface              0
      Endpoint Descriptor:
        bLength                 7
        bDescriptorType         5
        bEndpointAddress     0x81  EP 1 IN
        bmAttributes            2
          Transfer Type            Bulk
          Synch Type               None
          Usage Type               Data
        wMaxPacketSize     0x0200  1x 512 bytes
        bInterval               0
      Endpoint Descriptor:
        bLength                 7
        bDescriptorType         5
        bEndpointAddress     0x02  EP 2 OUT
        bmAttributes            2
          Transfer Type            Bulk
          Synch Type               None
          Usage Type               Data
        wMaxPacketSize     0x0200  1x 512 bytes
        bInterval               0
</pre>
