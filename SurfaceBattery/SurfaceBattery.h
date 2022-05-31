/*++

Copyright (c) Microsoft Corporation. All rights reserved.

Module Name:

    SurfaceBattery.h

Abstract:

    This is the header file for the Surface battery driver.

    N.B. This code is provided "AS IS" without any expressed or implied warranty.

--*/

//---------------------------------------------------------------------- Pragmas

#pragma once

//--------------------------------------------------------------------- Includes

#include <wdm.h>
#include <wdf.h>
#include <batclass.h>
#include <wmistr.h>
#include <wmilib.h>
#include <ntstrsafe.h>
#include "trace.h"
#define RESHUB_USE_HELPER_ROUTINES
#include <reshub.h>
#include "spb.h"

//--------------------------------------------------------------------- Literals

#define SURFACE_BATTERY_TAG                 'StaB'

//------------------------------------------------------------------ Definitions

typedef struct {
    UNICODE_STRING                  RegistryPath;
} SURFACE_BATTERY_GLOBAL_DATA, *PSURFACE_BATTERY_GLOBAL_DATA;

#define SURFACE_BATTERY_STATE_VERSION_1     1
#define SURFACE_BATTERY_STATE_VERSION       SURFACE_BATTERY_STATE_VERSION_1

#define SURFACE_BATTERY_RATE_CALCULATE      0x7fffffff

typedef struct {
    USHORT                          Version;
    BATTERY_MANUFACTURE_DATE        ManufactureDate;
    BATTERY_INFORMATION             BatteryInfo;
    BATTERY_STATUS                  BatteryStatus;
    BATTERY_REPORTING_SCALE         GranularityScale[4];
    ULONG                           GranularityCount;
    ULONG                           EstimatedTime;
    ULONG                           Temperature;
    ULONG                           MaxCurrentDraw;
    WCHAR                           DeviceName[MAX_BATTERY_STRING_SIZE];
    WCHAR                           ManufacturerName[MAX_BATTERY_STRING_SIZE];
    WCHAR                           SerialNumber[MAX_BATTERY_STRING_SIZE];
    WCHAR                           UniqueId[MAX_BATTERY_STRING_SIZE];
} SURFACE_BATTERY_STATE, * PSURFACE_BATTERY_STATE;

typedef struct {
    //
    // Device handle
    //
    WDFDEVICE Device;

    //
    // Battery class registration
    //

    PVOID                           ClassHandle;
    WDFWAITLOCK                     ClassInitLock;
    WMILIB_CONTEXT                  WmiLibContext;

    //
    // Spb (I2C) related members used for the lifetime of the device
    //
    SPB_CONTEXT I2CContext;

    //
    // Battery state
    //

    WDFWAITLOCK                     StateLock;
    ULONG                           BatteryTag;
    SURFACE_BATTERY_STATE           State;
} SURFACE_BATTERY_FDO_DATA, *PSURFACE_BATTERY_FDO_DATA;

//------------------------------------------------------ WDF Context Declaration

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(SURFACE_BATTERY_GLOBAL_DATA, GetGlobalData);
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(SURFACE_BATTERY_FDO_DATA, GetDeviceExtension);

//----------------------------------------------------- Prototypes (miniclass.c)

_IRQL_requires_same_
VOID
SurfaceBatteryPrepareHardware(
    _In_ WDFDEVICE Device
);

BCLASS_QUERY_TAG_CALLBACK SurfaceBatteryQueryTag;
BCLASS_QUERY_INFORMATION_CALLBACK SurfaceBatteryQueryInformation;
BCLASS_SET_INFORMATION_CALLBACK SurfaceBatterySetInformation;
BCLASS_QUERY_STATUS_CALLBACK SurfaceBatteryQueryStatus;
BCLASS_SET_STATUS_NOTIFY_CALLBACK SurfaceBatterySetStatusNotify;
BCLASS_DISABLE_STATUS_NOTIFY_CALLBACK SurfaceBatteryDisableStatusNotify;