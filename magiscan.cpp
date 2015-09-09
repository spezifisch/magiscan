/*
 * magiscan - Linux scan tool for Hyundai Magic Scan
 * Copyright (C) 2015 szf <spezifisch@users.noreply.github.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <scsi/sg.h>
#include <unistd.h>

//#define SERVICE_DEBUG 1
#define MEMREAD_DEBUG 1
#define SCSI_DEBUG 1

#ifdef SERVICE_DEBUG
#define PRINTF_SERVICE printf
#else
#define PRINTF_SERVICE
#endif

#define SG_IOBUF_LEN 100
#define SG_TIMEOUT 15000 // 15 seconds

void dumpHex(unsigned char *buf, unsigned buflen)
{
    if (!buf) {
        return;
    }

    for (unsigned i = 0; i < buflen; i++) {
        printf("%02x", buf[i]);
    }
    printf("\n");
}

bool lowlevelCmd(int fd, unsigned char *cdb_buf, unsigned cdb_len, unsigned char *dxf_buf, unsigned dxf_len)
{
    bool ret = false;
    unsigned char iobuf[SG_IOBUF_LEN] = { 0 };

    struct sg_io_hdr *p = (struct sg_io_hdr *) malloc(sizeof(struct sg_io_hdr));
    if (!p) {
        return ret;
    }
    memset(p, 0, sizeof(struct sg_io_hdr));

    p->interface_id = 'S';
    p->cmdp = cdb_buf;
    p->cmd_len = cdb_len;
    p->flags = SG_FLAG_LUN_INHIBIT;
    p->dxfer_direction = SG_DXFER_FROM_DEV;
    p->dxferp = dxf_buf;
    p->dxfer_len = dxf_len;
    p->sbp = iobuf;
    p->mx_sb_len = SG_IOBUF_LEN;
    p->timeout = SG_TIMEOUT;

    if (ioctl(fd, SG_IO, p) < 0) {
        printf("ioctl error: errno=%d (%s)\n", errno, strerror(errno));
        goto llcmd_fail;
    }

#ifdef SCSI_DEBUG
    printf("duration: %d ms status: 0x%x host_status 0x%x driver_status 0x%x resid %d\n",
        p->duration, p->status, p->host_status, p->driver_status, p->resid);
#endif

    if (p->resid == 0) {
        ret = true;
    }

llcmd_fail:
    if (p)
        free(p);

    return ret;
}

unsigned lowlevelServiceCmd(int fd, unsigned char code)
{
    unsigned char rxbuf[16] = { 0 };
    unsigned char cdb[16] = { 0xc5, 7, 0, 0, 0, 0, 0, 0, 0, 0x10, 0xff, code, 0, 0, 0, 0 };

    bool ok = lowlevelCmd(fd, cdb, sizeof(cdb), rxbuf, sizeof(rxbuf));
    if (!ok) {
        return -1;
    }

#ifdef SERVICE_DEBUG
    printf("lowlevelServiceCmd(code=%d): ", code);
    dumpHex(rxbuf, sizeof(rxbuf));
#endif

    // return first 4 bytes
    return *((unsigned*)rxbuf);
}

bool lowlevelReadCmd(int fd, unsigned char *buf, unsigned chunk_size, unsigned addr)
{
    // interpret arguments as arrays of bytes
    unsigned char cs[4] = { 0 };
    unsigned char ad[4] = { 0 };
    *((unsigned*)cs) = chunk_size;
    *((unsigned*)ad) = addr;

    // create cmd packet
    unsigned char cdb[16] = { 0xc3, 7, ad[3], ad[2], ad[1], ad[0], cs[3], cs[2], cs[1], cs[0], 0, 0, 0, 0, 0, 0 };

#ifdef MEMREAD_DEBUG
    printf("lowlevelReadCmd: ");
    dumpHex(cdb, sizeof(cdb));
#endif

    return lowlevelCmd(fd, cdb, sizeof(cdb), buf, chunk_size);
}

bool checkDevice(int fd)
{
    unsigned rxbuf = 0;
    unsigned char cdb[16] = { 0xc5, 7, 0, 0, 0, 0, 0, 0, 0, 4, 0xff, 2, 0, 0, 0, 0 };

    bool ok = lowlevelCmd(fd, cdb, sizeof(cdb), (unsigned char*)&rxbuf, 4);
    if (!ok) {
        return false;
    }

    if ((unsigned)rxbuf == 0x41564f4e) {
        return true;
    }

    printf("checkDevice fail: ");
    dumpHex((unsigned char*)&rxbuf, sizeof(rxbuf));

    return false;
}

bool ServiceIsFinished(int fd)
{
    unsigned char ret = lowlevelServiceCmd(fd, 0x04);
    return (ret == 0x10);
}

bool ServiceIsLocked(int fd)
{
    unsigned char ret = lowlevelServiceCmd(fd, 0x03);
    return !(ret == 0x10);
}

bool ServiceLock(int fd)
{
    unsigned char ret = lowlevelServiceCmd(fd, 0x05);
    return (ret == 0x10);
}

bool ServiceUnlock(int fd)
{
    unsigned char ret = lowlevelServiceCmd(fd, 0x06);
    return !(ret == 0x10);
}

bool ServiceOpen(int fd)
{
    PRINTF_SERVICE("ServiceOpen: waiting for unlock\n");

    while (ServiceIsFinished(fd)) {
        if (!ServiceIsLocked(fd)) {
            PRINTF_SERVICE("ServiceOpen: not locked\n");
            break;
        }

        printf(".");
        usleep(10000);
    }

    if (ServiceIsLocked(fd)) {
        PRINTF_SERVICE("ServiceOpen: unlocking\n");
        ServiceUnlock(fd);
    }

    while (1) {
        if (ServiceLock(fd)) {
            PRINTF_SERVICE("ServiceOpen: locked\n");
            return true;
        }

        printf(".");
        usleep(10000);
    }

    return false;
}

bool ServiceClose(int fd)
{
    PRINTF_SERVICE("ServiceClose: waiting for finish\n");

    while (!ServiceIsFinished(fd)) {
        printf(".");
        usleep(10000);
    }

    PRINTF_SERVICE("ServiceClose: unlock\n");
    return ServiceUnlock(fd);
}

bool VendorCmdGetData(int fd, unsigned char code, unsigned char *buf, unsigned char buflen)
{
    bool ret = false;

    memset(buf, 0, buflen);

    if (!ServiceOpen(fd)) {
        return false;
    }

    unsigned char cdb[16] = { 0xc5, 7, 0, 0, 0, 0, 0, 0, 0, buflen, 2, 1, code, 0, 0, 0 };

    bool ok = lowlevelCmd(fd, cdb, sizeof(cdb), buf, buflen);
    if (!ok) {
        printf("VendorCmdGetData fail 1\n");
        goto vcgd_fail;
    }
    printf("VendorCmdGetData(code=%d)#1: ", code);
    dumpHex(buf, buflen);

    ServiceIsFinished(fd);

    cdb[11] = 2;
    ok = lowlevelCmd(fd, cdb, sizeof(cdb), buf, buflen);
    if (!ok) {
        printf("VendorCmdGetData fail 2\n");
        goto vcgd_fail;
    }
    printf("VendorCmdGetData(code=%d)#1: ", code);
    dumpHex(buf, buflen);

    ret = true;

vcgd_fail:
    ServiceClose(fd);
    return ret;
}

// enums suffixed with "xx" are not original
typedef enum {
    STATE_IDLE, STATE_SCAN, STATE_AUTOCAL, STATE_AUTOCAL_MOVE, STATE_NOT_CAL, STATE_INVALIDxx
} DeviceState_t;

char state_lut[][13] = { "IDLE", "SCAN", "AUTOCAL", "AUTOCAL_MOVE", "NOT_CAL" };

DeviceState_t GetDeviceState(int fd)
{
    unsigned buf = 0;

    if (!VendorCmdGetData(fd, 0, (unsigned char*)&buf, 4)) {
        return STATE_INVALIDxx;
    }

    printf("GetDeviceState: ");
    dumpHex((unsigned char*)&buf, 4);

    return (DeviceState_t)buf;
}

char status_lut[][13] = { "NOT_READY", "READY", "OVERSPEED" };

typedef enum {
    DI_STATUS_NOT_READY,
    DI_STATUS_READY,
    DI_STATUS_OVERSPEED,
    DI_STATUS_INVALIDxx
} DI_Status_t;

typedef enum {
    DI_COLOR_GRAY,
    DI_COLOR_COLOR
} DI_Color_t;

typedef enum {
    DI_DPI_L,
    DI_DPI_M
} DI_DPI_t;

typedef struct {
    DI_Status_t status;
    DI_Color_t color;
    DI_DPI_t dpi;
    int width;
    int height;
    int addr;
} DataInfo_t;

bool GetDeviceDataInfo(int fd, DataInfo_t *out)
{
    return VendorCmdGetData(fd, 1, (unsigned char*)out, sizeof(DataInfo_t));
}

void dumpDataInfo(DataInfo_t *p)
{
    printf("DataInfo status:%d (%s) color:%d dpi:%d width:%d height:%d addr:%x\n",
        p->status, status_lut[p->status], p->color, p->dpi, p->width, p-> height, p->addr);
}

bool MemoryRead(int fd, int addr, unsigned char *imgbuf, unsigned imgbufsize)
{
    unsigned chunk_size = 0x10000;

    if (chunk_size > imgbufsize) {
        chunk_size = imgbufsize;
    }

    unsigned char *buf = imgbuf;
    int remaining_bytes = imgbufsize;

    // omitted weird handling when (addr&3) == true

    while (remaining_bytes > 0) {
        // read chunk of data
        bool ok = lowlevelReadCmd(fd, buf, chunk_size, addr);
        if (!ok) {
            printf("MemoryRead fail\n");
            return false;
        }

        printf("read chunk of 0x%x bytes, start addr 0x%x, bufptr 0x%p\n", chunk_size, addr, (void*)buf);
        remaining_bytes -= chunk_size;

        // pointer to next data
        buf += chunk_size;
        addr += chunk_size;

        if (chunk_size > remaining_bytes) {
            chunk_size = remaining_bytes;
        }
    }

    return true;
}

bool shuffleBitmap(unsigned char *imgbuf, unsigned imgbufsize, int width, int height, DI_Color_t color)
{
    unsigned char *dst = new unsigned char[imgbufsize];
    if (!dst) {
        return false;
    }

    unsigned pos = 0, index = 0;
    if (color == DI_COLOR_COLOR) {
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                dst[pos++] = imgbuf[index];
                dst[pos++] = imgbuf[index + width];
                dst[pos++] = imgbuf[index + 2*width];
                index++;
            }

            index += 2*width;
        }
    } else {
    }

    memcpy(imgbuf, dst, imgbufsize);

    delete [] dst;
    dst = 0;
    return true;
}

int main(int argc, char **argv)
{
    char cDiskName[] = "/dev/sg2";
    int fd = open(cDiskName, O_RDWR);
    if (fd < 0)
    {
        printf("Open error: %s, errno=%d (%s)\n", cDiskName, errno, strerror(errno));
        return 1;
    }

    if (!checkDevice(fd)) {
        printf("checkDevice fail\n");
        return 2;
    }
    printf("init ok\n");

    DataInfo_t datainfo;
    int last_addr = 0;
    DeviceState_t last_state = STATE_INVALIDxx;
    DI_Status_t last_status = DI_STATUS_INVALIDxx;

    unsigned scanned_imgs = 0;

    while (true) {
        // get state: scanning or idle
        DeviceState_t state = GetDeviceState(fd);
        if (state >= STATE_INVALIDxx || state < 0) {
            return 7;
        }
        if (state != last_state) {
            printf("DeviceState: %d => %s\n", (unsigned)state, state_lut[state]);
        }

        // get data info: new data ready or not?
        if (!GetDeviceDataInfo(fd, &datainfo)) {
            return 8;
        }
        if (datainfo.status < 0 || datainfo.status > 2) {
            return 9;
        }
        if (datainfo.status != last_status || datainfo.addr != last_addr) {
            dumpDataInfo(&datainfo);
        }

        if (datainfo.status != DI_STATUS_NOT_READY) {
            // there's data for us
            unsigned imgbufsize = datainfo.width * datainfo.height;
            if (datainfo.color == DI_COLOR_COLOR) {
                imgbufsize *= 3;
            }

            // image buffer
            unsigned char *imgbuf = new unsigned char[imgbufsize];

            // read data
            bool ok = MemoryRead(fd, datainfo.addr, imgbuf, imgbufsize);
            if (ok) {
                printf("read %d bytes\n", imgbufsize);

                shuffleBitmap(imgbuf, imgbufsize, datainfo.width, datainfo.height, datainfo.color);

                int ifd;
                if (!scanned_imgs) {
                    ifd = open("out.data", O_WRONLY|O_CREAT|O_TRUNC, 0644);
                } else {
                    ifd = open("out.data", O_WRONLY|O_APPEND);
                }

                if (ifd) {
                    write(ifd, imgbuf, imgbufsize);
                    close(ifd);

                    scanned_imgs++;
                }
            } else {
                printf("failed reading data\n");
            }

            delete [] imgbuf;
            imgbuf = 0;
        }

        usleep(20000);
        last_state = state;
        last_status = datainfo.status;
        last_addr = datainfo.addr;
    }

    close(fd);
    printf("all ok\n");
    return 0;
}
