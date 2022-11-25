/*++

Copyright (c) Microsoft Corporation. All rights reserved.

Module Name:

    HotdogBattery.h

Abstract:

    This is the header file for the Hotdog battery driver.

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

/*
* Rob Green, a member of the NTDEV list, provides the
* following set of macros that'll keep you from having
* to scratch your head and count zeros ever again.
* Using these defintions, all you'll have to do is write:
*
* interval.QuadPart = RELATIVE(SECONDS(5));
*/

#ifndef ABSOLUTE
#define ABSOLUTE(wait) (wait)
#endif

#ifndef RELATIVE
#define RELATIVE(wait) (-(wait))
#endif

#ifndef NANOSECONDS
#define NANOSECONDS(nanos) \
	(((signed __int64)(nanos)) / 100L)
#endif

#ifndef MICROSECONDS
#define MICROSECONDS(micros) \
	(((signed __int64)(micros)) * NANOSECONDS(1000L))
#endif

#ifndef MILLISECONDS
#define MILLISECONDS(milli) \
	(((signed __int64)(milli)) * MICROSECONDS(1000L))
#endif

#ifndef SECONDS
#define SECONDS(seconds) \
	(((signed __int64)(seconds)) * MILLISECONDS(1000L))
#endif

//------------------------------------------------------------------ Definitions

#define MFG_NAME_SIZE  0x3
#define DEVICE_NAME_SIZE 0x8
#define CHEM_SIZE 0x4

#pragma pack(push, 1)
//typedef struct _BQ27742_MANUF_INFO_TYPE
//{
//    UINT16 BatteryManufactureDate;
//    UINT32 BatterySerialNumber;
//    BYTE BatteryManufactureName[MFG_NAME_SIZE];
//    BYTE BatteryDeviceName[DEVICE_NAME_SIZE];
//    BYTE Chemistry[CHEM_SIZE];
//} BQ27742_MANUF_INFO_TYPE, * PBQ27742_MANUF_INFO_TYPE;
#pragma pack(pop)


typedef struct {
    UNICODE_STRING                  RegistryPath;
} SURFACE_BATTERY_GLOBAL_DATA, *PSURFACE_BATTERY_GLOBAL_DATA;

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
} SURFACE_BATTERY_FDO_DATA, *PSURFACE_BATTERY_FDO_DATA;

//------------------------------------------------------ WDF Context Declaration

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(SURFACE_BATTERY_GLOBAL_DATA, GetGlobalData);
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(SURFACE_BATTERY_FDO_DATA, GetDeviceExtension);

//----------------------------------------------------- Prototypes (miniclass.c)

_IRQL_requires_same_
VOID
HotdogBatteryPrepareHardware(
    _In_ WDFDEVICE Device
);

BCLASS_QUERY_TAG_CALLBACK HotdogBatteryQueryTag;
BCLASS_QUERY_INFORMATION_CALLBACK HotdogBatteryQueryInformation;
BCLASS_SET_INFORMATION_CALLBACK HotdogBatterySetInformation;
BCLASS_QUERY_STATUS_CALLBACK HotdogBatteryQueryStatus;
BCLASS_SET_STATUS_NOTIFY_CALLBACK HotdogBatterySetStatusNotify;
BCLASS_DISABLE_STATUS_NOTIFY_CALLBACK HotdogBatteryDisableStatusNotify;