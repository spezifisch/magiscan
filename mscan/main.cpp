/*
 * magiscan - Windows scan tool for Hyundai Magic Scan using original NvUSB.dll
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

#include <iostream>
#include <windows.h>
#include <conio.h>

#include "nvusb.h"
#include "bmp.h"

using namespace std;

// imported functions
typedef int (*NvUSB_GetFirstAvailableDevice_t)(void);
NvUSB_GetFirstAvailableDevice_t NvUSB_GetFirstAvailableDevice;

typedef int (*NvUSB_ConnectToDevice_t)(int);
NvUSB_ConnectToDevice_t NvUSB_ConnectToDevice;

typedef int (*NvUSB_VenderCmd_GetData_t)(int, char, void *, unsigned);
NvUSB_VenderCmd_GetData_t NvUSB_VenderCmd_GetData;

typedef int (*NvUSB_MemoryRead_t)(int, unsigned, unsigned, void*);
NvUSB_MemoryRead_t NvUSB_MemoryRead;

string DeviceState_LUT[] = { "idle", "scan", "autocal", "autocal_move", "not_cal" };
string DataStatus_LUT[] = { "not ready", "ready", "overspeed" };

int GetDeviceState(int devhandle)
{
    int buf = 0;
    bool ok = NvUSB_VenderCmd_GetData(devhandle, 0, &buf, 4);
    if (!ok) {
        return -1;
    }
    return buf;
}

bool GetDeviceDataInfo(int devhandle, MSDC_VENDOR_DATAINFO_STRUCT_t *out)
{
    return NvUSB_VenderCmd_GetData(devhandle, 1, out, sizeof(MSDC_VENDOR_DATAINFO_STRUCT_t));
}

int main(void)
{
    // load scanner DLL
    const char dllfn[] = "../../NvUSB.dll";
    cout << "Loading " << dllfn << endl;

    HMODULE dllh = LoadLibrary(dllfn);
    if (!dllh) {
        return 1;
    }

    // get function pointers
    NvUSB_GetFirstAvailableDevice = (NvUSB_GetFirstAvailableDevice_t)GetProcAddress(dllh, "NvUSB_GetFirstAvailableDevice");
    if (!NvUSB_GetFirstAvailableDevice) {
        return 2;
    }
    NvUSB_ConnectToDevice = (NvUSB_ConnectToDevice_t)GetProcAddress(dllh, "NvUSB_ConnectToDevice");
    if (!NvUSB_ConnectToDevice) {
        return 3;
    }
    NvUSB_VenderCmd_GetData = (NvUSB_VenderCmd_GetData_t)GetProcAddress(dllh, "NvUSB_VenderCmd_GetData");
    if (!NvUSB_VenderCmd_GetData) {
        return 4;
    }
    NvUSB_MemoryRead = (NvUSB_MemoryRead_t)GetProcAddress(dllh, "NvUSB_MemoryRead");
    if (!NvUSB_MemoryRead) {
        return 4;
    }

    // check if scanner present
    int devhandle = NvUSB_GetFirstAvailableDevice();
    cout << "NvUSB_GetFirstAvailableDevice = " << devhandle << endl;
    if (!devhandle) {
        cerr << "no device found" << endl;
        return 4;
    }

    int ret = NvUSB_ConnectToDevice(devhandle);
    if (!ret) {
        cerr << "connect to device failed" << endl;
        return 4;
    }

    // scan loop ...
    MSDC_VENDOR_DATAINFO_STRUCT_t datainfo;
    MSDC_VENDOR_DATA_STATUS_ENUM_t last_distatus = MSDC_VENDOR_DATA_STATUS_INVALID_pb;

    unsigned state, last_state = -1;
    while (true) {
        // get scanning/button state
        state = GetDeviceState(devhandle);
        if (state < 0 || state >= sizeof(DeviceState_LUT)) {
            return 5;
        }
        if (state != last_state) {
            cout << "Device State: " << state << " => " << DeviceState_LUT[state] << endl;
        }

        if (state == 0) { // IDLE

        } else if (state == 1) { // SCAN
            // we're scanning. check if data is ready
            memset((void*)&datainfo, 0, sizeof(datainfo));
            if (!GetDeviceDataInfo(devhandle, &datainfo)) {
                return 6;
            }
            if (datainfo.Status < 0 || datainfo.Status >= sizeof(DataStatus_LUT)) {
                return 7;
            }
            if (last_distatus != datainfo.Status) {
                cout << "Data Status: " << datainfo.Status << " (" << DataStatus_LUT[datainfo.Status];
                cout << ") Color " << datainfo.Color << " Dpi " << datainfo.Dpi;
                cout << " Width " << datainfo.Width << " Height " << datainfo.Height;
                cout << " Addr " << datainfo.Addr << endl;
            }

            // process data
            if (datainfo.Status != MSDC_VENDOR_DATA_STATUS_NOT_READY) {
                // there is data, create buffer for it
                unsigned bufsize = datainfo.Width * datainfo.Height;
                if (datainfo.Color == MSDC_VENDOR_COLOR_COLOR) {
                    bufsize *= 3;
                }
                BYTE *buf = new BYTE[bufsize];

                // read data
                bool ok = NvUSB_MemoryRead(devhandle, datainfo.Addr, bufsize, buf);
                if (ok) {
                    cout << "- read " << bufsize << " Bytes" << endl;

                    if (datainfo.Color == MSDC_VENDOR_COLOR_COLOR) {
                        SaveBitmapToFileColor(buf, datainfo.Width, datainfo.Height, 24, "out_color.bmp");
                    } else {
                        SaveBitmapToFile(buf, datainfo.Width, datainfo.Height, 24, "out_bw.bmp");
                    }
                } else {
                    cerr << "- failed reading data" << endl;
                }

                delete [] buf;
                buf = NULL;
            }

            last_distatus = datainfo.Status;
        }

        // wait
        Sleep(10);

        // exit on key press
        if (kbhit()) {
            cout << "key pressed!" << endl;
            break;
        }

        last_state = state;
    }

    cout << "ok" << endl;
    return 0;
}
