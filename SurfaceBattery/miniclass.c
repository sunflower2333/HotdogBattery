/*++

Copyright (c) Microsoft Corporation. All rights reserved.

Module Name:

	miniclass.c

Abstract:

	This module implements battery miniclass functionality specific to the
	Surface battery driver.

	N.B. This code is provided "AS IS" without any expressed or implied warranty.

--*/

//--------------------------------------------------------------------- Includes

#include "SurfaceBattery.h"
#include "miniclass.tmh"

//--------------------------------------------------------------------- Literals

#define DEFAULT_NAME                L"SimulatedBattery"
#define DEFAULT_MANUFACTURER        L"Microsoft Corp"
#define DEFAULT_SERIALNO            L"0000"
#define DEFAULT_UNIQUEID            L"SimulatedBattery0000"

//------------------------------------------------------------------- Prototypes

_IRQL_requires_same_
VOID
SurfaceBatteryUpdateTag(
	_Inout_ PSURFACE_BATTERY_FDO_DATA DevExt
);

BCLASS_QUERY_TAG_CALLBACK SurfaceBatteryQueryTag;
BCLASS_QUERY_INFORMATION_CALLBACK SurfaceBatteryQueryInformation;
BCLASS_SET_INFORMATION_CALLBACK SurfaceBatterySetInformation;
BCLASS_QUERY_STATUS_CALLBACK SurfaceBatteryQueryStatus;
BCLASS_SET_STATUS_NOTIFY_CALLBACK SurfaceBatterySetStatusNotify;
BCLASS_DISABLE_STATUS_NOTIFY_CALLBACK SurfaceBatteryDisableStatusNotify;

_Success_(return == STATUS_SUCCESS)
NTSTATUS
SurfaceBatterySetBatteryString(
	_In_ PCWSTR String,
	_Out_writes_(MAX_BATTERY_STRING_SIZE) PWCHAR Destination
);

//---------------------------------------------------------------------- Pragmas

#pragma alloc_text(PAGE, SurfaceBatteryPrepareHardware)
#pragma alloc_text(PAGE, SurfaceBatteryUpdateTag)
#pragma alloc_text(PAGE, SurfaceBatteryQueryTag)
#pragma alloc_text(PAGE, SurfaceBatteryQueryInformation)
#pragma alloc_text(PAGE, SurfaceBatteryQueryStatus)
#pragma alloc_text(PAGE, SurfaceBatterySetStatusNotify)
#pragma alloc_text(PAGE, SurfaceBatteryDisableStatusNotify)
#pragma alloc_text(PAGE, SurfaceBatterySetInformation)

//------------------------------------------------------------ Battery Interface
_Use_decl_annotations_
VOID
SurfaceBatteryPrepareHardware(
	WDFDEVICE Device
)

/*++

Routine Description:

	This routine is called to initialize battery data to sane values.

	A real battery would query hardware to determine if a battery is present,
	query its static capabilities, etc.

Arguments:

	Device - Supplies the device to initialize.

Return Value:

	NTSTATUS

--*/

{

	PSURFACE_BATTERY_FDO_DATA DevExt;
	NTSTATUS Status = STATUS_SUCCESS;

	Trace(TRACE_LEVEL_INFORMATION, SURFACE_BATTERY_TRACE, "Entering " __FUNCTION__ "\n");
	PAGED_CODE();

	DevExt = GetDeviceExtension(Device);

	WdfWaitLockAcquire(DevExt->StateLock, NULL);
	SurfaceBatteryUpdateTag(DevExt);
	DevExt->State.Version = SURFACE_BATTERY_STATE_VERSION;
	DevExt->State.BatteryStatus.PowerState = BATTERY_POWER_ON_LINE;
	DevExt->State.BatteryStatus.Capacity = 100;
	DevExt->State.BatteryStatus.Voltage = BATTERY_UNKNOWN_VOLTAGE;
	DevExt->State.BatteryStatus.Rate = 0;
	DevExt->State.BatteryInfo.Capabilities = BATTERY_SYSTEM_BATTERY |
		BATTERY_CAPACITY_RELATIVE;

	DevExt->State.BatteryInfo.Technology = 1;
	DevExt->State.BatteryInfo.Chemistry[0] = 'F';
	DevExt->State.BatteryInfo.Chemistry[1] = 'a';
	DevExt->State.BatteryInfo.Chemistry[2] = 'k';
	DevExt->State.BatteryInfo.Chemistry[3] = 'e';
	DevExt->State.BatteryInfo.DesignedCapacity = 100;
	DevExt->State.BatteryInfo.FullChargedCapacity = 100;
	DevExt->State.BatteryInfo.DefaultAlert1 = 0;
	DevExt->State.BatteryInfo.DefaultAlert2 = 0;
	DevExt->State.BatteryInfo.CriticalBias = 0;
	DevExt->State.BatteryInfo.CycleCount = 100;
	DevExt->State.MaxCurrentDraw = UNKNOWN_CURRENT;
	SurfaceBatterySetBatteryString(DEFAULT_NAME, DevExt->State.DeviceName);
	SurfaceBatterySetBatteryString(DEFAULT_MANUFACTURER,
		DevExt->State.ManufacturerName);

	SurfaceBatterySetBatteryString(DEFAULT_SERIALNO, DevExt->State.SerialNumber);
	SurfaceBatterySetBatteryString(DEFAULT_UNIQUEID, DevExt->State.UniqueId);
	WdfWaitLockRelease(DevExt->StateLock);

	Trace(TRACE_LEVEL_INFORMATION, SURFACE_BATTERY_TRACE,
		"Leaving " __FUNCTION__ ": Status=0x%x\n",
		Status);
	return;
}

_Use_decl_annotations_
VOID
SurfaceBatteryUpdateTag(
	PSURFACE_BATTERY_FDO_DATA DevExt
)

/*++

Routine Description:

	This routine is called when static battery properties have changed to
	update the battery tag.

Arguments:

	DevExt - Supplies a pointer to the device extension  of the battery to
		update.

Return Value:

	None

--*/

{

	PAGED_CODE();

	DevExt->BatteryTag += 1;
	if (DevExt->BatteryTag == BATTERY_TAG_INVALID) {
		DevExt->BatteryTag += 1;
	}

	return;
}

_Use_decl_annotations_
NTSTATUS
SurfaceBatteryQueryTag(
	PVOID Context,
	PULONG BatteryTag
)

/*++

Routine Description:

	This routine is called to get the value of the current battery tag.

Arguments:

	Context - Supplies the miniport context value for battery

	BatteryTag - Supplies a pointer to a ULONG to receive the battery tag.

Return Value:

	NTSTATUS

--*/

{
	PSURFACE_BATTERY_FDO_DATA DevExt;
	NTSTATUS Status;

	Trace(TRACE_LEVEL_INFORMATION, SURFACE_BATTERY_TRACE, "Entering " __FUNCTION__ "\n");
	PAGED_CODE();

	DevExt = (PSURFACE_BATTERY_FDO_DATA)Context;
	WdfWaitLockAcquire(DevExt->StateLock, NULL);
	*BatteryTag = DevExt->BatteryTag;
	WdfWaitLockRelease(DevExt->StateLock);
	if (*BatteryTag == BATTERY_TAG_INVALID) {
		Status = STATUS_NO_SUCH_DEVICE;
	}
	else {
		Status = STATUS_SUCCESS;
	}

	Trace(TRACE_LEVEL_INFORMATION, SURFACE_BATTERY_TRACE,
		"Leaving " __FUNCTION__ ": Status=0x%x\n",
		Status);
	return Status;
}

_Use_decl_annotations_
NTSTATUS
SurfaceBatteryQueryInformation(
	PVOID Context,
	ULONG BatteryTag,
	BATTERY_QUERY_INFORMATION_LEVEL Level,
	LONG AtRate,
	PVOID Buffer,
	ULONG BufferLength,
	PULONG ReturnedLength
)

/*++

Routine Description:

	Called by the class driver to retrieve battery information

	The battery class driver will serialize all requests it issues to
	the miniport for a given battery.

	Return invalid parameter when a request for a specific level of information
	can't be handled. This is defined in the battery class spec.

Arguments:

	Context - Supplies the miniport context value for battery

	BatteryTag - Supplies the tag of current battery

	Level - Supplies the type of information required

	AtRate - Supplies the rate of drain for the BatteryEstimatedTime level

	Buffer - Supplies a pointer to a buffer to place the information

	BufferLength - Supplies the length in bytes of the buffer

	ReturnedLength - Supplies the length in bytes of the returned data

Return Value:

	Success if there is a battery currently installed, else no such device.

--*/

{
	PSURFACE_BATTERY_FDO_DATA DevExt;
	ULONG ResultValue;
	PVOID ReturnBuffer;
	size_t ReturnBufferLength;
	NTSTATUS Status;

	UNREFERENCED_PARAMETER(AtRate);

	Trace(TRACE_LEVEL_INFORMATION, SURFACE_BATTERY_TRACE, "Entering " __FUNCTION__ "\n");
	PAGED_CODE();

	DevExt = (PSURFACE_BATTERY_FDO_DATA)Context;
	WdfWaitLockAcquire(DevExt->StateLock, NULL);
	if (BatteryTag != DevExt->BatteryTag) {
		Status = STATUS_NO_SUCH_DEVICE;
		goto QueryInformationEnd;
	}

	//
	// Determine the value of the information being queried for and return it.
	// In a real battery, this would require hardware/firmware accesses. The
	// simulated battery fakes this by storing the data to be returned in
	// memory.
	//

	ReturnBuffer = NULL;
	ReturnBufferLength = 0;
	Trace(TRACE_LEVEL_INFORMATION, SURFACE_BATTERY_INFO, "Query for information level 0x%x\n", Level);
	Status = STATUS_INVALID_DEVICE_REQUEST;
	switch (Level) {
	case BatteryInformation:
		ReturnBuffer = &DevExt->State.BatteryInfo;
		ReturnBufferLength = sizeof(BATTERY_INFORMATION);
		Status = STATUS_SUCCESS;
		break;

	case BatteryEstimatedTime:
		if (DevExt->State.EstimatedTime == SURFACE_BATTERY_RATE_CALCULATE) {
			if (AtRate == 0) {
				AtRate = DevExt->State.BatteryStatus.Rate;
			}

			if (AtRate < 0) {
				ResultValue = (3600 * DevExt->State.BatteryStatus.Capacity) /
					(-AtRate);

			}
			else {
				ResultValue = BATTERY_UNKNOWN_TIME;
			}

		}
		else {
			ResultValue = DevExt->State.EstimatedTime;
		}

		ReturnBuffer = &ResultValue;
		ReturnBufferLength = sizeof(ResultValue);
		Status = STATUS_SUCCESS;
		break;

	case BatteryUniqueID:
		ReturnBuffer = DevExt->State.UniqueId;
		Status = RtlStringCbLengthW(DevExt->State.UniqueId,
			sizeof(DevExt->State.UniqueId),
			&ReturnBufferLength);

		ReturnBufferLength += sizeof(WCHAR);
		break;

	case BatteryManufactureName:
		ReturnBuffer = DevExt->State.ManufacturerName;
		Status = RtlStringCbLengthW(DevExt->State.ManufacturerName,
			sizeof(DevExt->State.ManufacturerName),
			&ReturnBufferLength);

		ReturnBufferLength += sizeof(WCHAR);
		break;

	case BatteryDeviceName:
		ReturnBuffer = DevExt->State.DeviceName;
		Status = RtlStringCbLengthW(DevExt->State.DeviceName,
			sizeof(DevExt->State.DeviceName),
			&ReturnBufferLength);

		ReturnBufferLength += sizeof(WCHAR);
		break;

	case BatterySerialNumber:
		ReturnBuffer = DevExt->State.SerialNumber;
		Status = RtlStringCbLengthW(DevExt->State.SerialNumber,
			sizeof(DevExt->State.SerialNumber),
			&ReturnBufferLength);

		ReturnBufferLength += sizeof(WCHAR);
		break;

	case BatteryManufactureDate:
		if (DevExt->State.ManufactureDate.Day != 0) {
			ReturnBuffer = &DevExt->State.ManufactureDate;
			ReturnBufferLength = sizeof(BATTERY_MANUFACTURE_DATE);
			Status = STATUS_SUCCESS;
		}

		break;

	case BatteryGranularityInformation:
		if (DevExt->State.GranularityCount > 0) {
			ReturnBuffer = DevExt->State.GranularityScale;
			ReturnBufferLength = DevExt->State.GranularityCount *
				sizeof(BATTERY_REPORTING_SCALE);

			Status = STATUS_SUCCESS;
		}

		break;

	case BatteryTemperature:
		ReturnBuffer = &DevExt->State.Temperature;
		ReturnBufferLength = sizeof(ULONG);
		Status = STATUS_SUCCESS;
		break;

	default:
		Status = STATUS_INVALID_PARAMETER;
		break;
	}

	NT_ASSERT(((ReturnBufferLength == 0) && (ReturnBuffer == NULL)) ||
		((ReturnBufferLength > 0) && (ReturnBuffer != NULL)));

	if (NT_SUCCESS(Status)) {
		*ReturnedLength = (ULONG)ReturnBufferLength;
		if (ReturnBuffer != NULL) {
			if ((Buffer == NULL) || (BufferLength < ReturnBufferLength)) {
				Status = STATUS_BUFFER_TOO_SMALL;

			}
			else {
				memcpy(Buffer, ReturnBuffer, ReturnBufferLength);
			}
		}

	}
	else {
		*ReturnedLength = 0;
	}

QueryInformationEnd:
	WdfWaitLockRelease(DevExt->StateLock);
	Trace(TRACE_LEVEL_INFORMATION, SURFACE_BATTERY_TRACE,
		"Leaving " __FUNCTION__ ": Status=0x%x\n",
		Status);
	return Status;
}

_Use_decl_annotations_
NTSTATUS
SurfaceBatteryQueryStatus(
	PVOID Context,
	ULONG BatteryTag,
	PBATTERY_STATUS BatteryStatus
)

/*++

Routine Description:

	Called by the class driver to retrieve the batteries current status

	The battery class driver will serialize all requests it issues to
	the miniport for a given battery.

Arguments:

	Context - Supplies the miniport context value for battery

	BatteryTag - Supplies the tag of current battery

	BatteryStatus - Supplies a pointer to the structure to return the current
		battery status in

Return Value:

	Success if there is a battery currently installed, else no such device.

--*/

{
	PSURFACE_BATTERY_FDO_DATA DevExt;
	NTSTATUS Status;

	Trace(TRACE_LEVEL_INFORMATION, SURFACE_BATTERY_TRACE, "Entering " __FUNCTION__ "\n");
	PAGED_CODE();

	DevExt = (PSURFACE_BATTERY_FDO_DATA)Context;
	WdfWaitLockAcquire(DevExt->StateLock, NULL);
	if (BatteryTag != DevExt->BatteryTag) {
		Status = STATUS_NO_SUCH_DEVICE;
		goto QueryStatusEnd;
	}

	RtlCopyMemory(BatteryStatus,
		&DevExt->State.BatteryStatus,
		sizeof(BATTERY_STATUS));

	Status = STATUS_SUCCESS;

QueryStatusEnd:
	WdfWaitLockRelease(DevExt->StateLock);
	Trace(TRACE_LEVEL_INFORMATION, SURFACE_BATTERY_TRACE,
		"Leaving " __FUNCTION__ ": Status=0x%x\n",
		Status);
	return Status;
}

_Use_decl_annotations_
NTSTATUS
SurfaceBatterySetStatusNotify(
	PVOID Context,
	ULONG BatteryTag,
	PBATTERY_NOTIFY BatteryNotify
)

/*++

Routine Description:

	Called by the class driver to set the capacity and power state levels
	at which the class driver requires notification.

	The battery class driver will serialize all requests it issues to
	the miniport for a given battery.

Arguments:

	Context - Supplies the miniport context value for battery

	BatteryTag - Supplies the tag of current battery

	BatteryNotify - Supplies a pointer to a structure containing the
		notification critera.

Return Value:

	Success if there is a battery currently installed, else no such device.

--*/

{
	PSURFACE_BATTERY_FDO_DATA DevExt;
	NTSTATUS Status;

	UNREFERENCED_PARAMETER(BatteryNotify);

	Trace(TRACE_LEVEL_INFORMATION, SURFACE_BATTERY_TRACE, "Entering " __FUNCTION__ "\n");
	PAGED_CODE();

	DevExt = (PSURFACE_BATTERY_FDO_DATA)Context;
	WdfWaitLockAcquire(DevExt->StateLock, NULL);
	if (BatteryTag != DevExt->BatteryTag) {
		Status = STATUS_NO_SUCH_DEVICE;
		goto SetStatusNotifyEnd;
	}

	Status = STATUS_NOT_SUPPORTED;

SetStatusNotifyEnd:
	WdfWaitLockRelease(DevExt->StateLock);
	Trace(TRACE_LEVEL_INFORMATION, SURFACE_BATTERY_TRACE,
		"Leaving " __FUNCTION__ ": Status=0x%x\n",
		Status);
	return Status;
}

_Use_decl_annotations_
NTSTATUS
SurfaceBatteryDisableStatusNotify(
	PVOID Context
)

/*++

Routine Description:

	Called by the class driver to disable notification.

	The battery class driver will serialize all requests it issues to
	the miniport for a given battery.

Arguments:

	Context - Supplies the miniport context value for battery

Return Value:

	Success if there is a battery currently installed, else no such device.

--*/

{
	NTSTATUS Status;

	UNREFERENCED_PARAMETER(Context);

	Trace(TRACE_LEVEL_INFORMATION, SURFACE_BATTERY_TRACE, "Entering " __FUNCTION__ "\n");
	PAGED_CODE();

	Status = STATUS_NOT_SUPPORTED;
	Trace(TRACE_LEVEL_INFORMATION, SURFACE_BATTERY_TRACE,
		"Leaving " __FUNCTION__ ": Status=0x%x\n",
		Status);
	return Status;
}

_Use_decl_annotations_
NTSTATUS
SurfaceBatterySetInformation(
	PVOID Context,
	ULONG BatteryTag,
	BATTERY_SET_INFORMATION_LEVEL Level,
	PVOID Buffer
)

/*
 Routine Description:

	Called by the class driver to set the battery's charge/discharge state,
	critical bias, or charge current.

Arguments:

	Context - Supplies the miniport context value for battery

	BatteryTag - Supplies the tag of current battery

	Level - Supplies action requested

	Buffer - Supplies a critical bias value if level is BatteryCriticalBias.

Return Value:

	NTSTATUS

--*/

{
	PBATTERY_CHARGING_SOURCE ChargingSource;
	PSURFACE_BATTERY_FDO_DATA DevExt;
	NTSTATUS Status;

	Trace(TRACE_LEVEL_INFORMATION, SURFACE_BATTERY_TRACE, "Entering " __FUNCTION__ "\n");
	PAGED_CODE();

	DevExt = (PSURFACE_BATTERY_FDO_DATA)Context;
	WdfWaitLockAcquire(DevExt->StateLock, NULL);
	if (BatteryTag != DevExt->BatteryTag) {
		Status = STATUS_NO_SUCH_DEVICE;
		goto SetInformationEnd;
	}

	if (Buffer == NULL) {
		Status = STATUS_INVALID_PARAMETER_4;

	}
	else if (Level == BatteryChargingSource) {
		ChargingSource = (PBATTERY_CHARGING_SOURCE)Buffer;
		DevExt->State.MaxCurrentDraw = ChargingSource->MaxCurrent;
		Trace(TRACE_LEVEL_INFORMATION, SURFACE_BATTERY_INFO,
			"SurfaceBattery : Set MaxCurrentDraw = %u mA\n",
			DevExt->State.MaxCurrentDraw);

		Status = STATUS_SUCCESS;
	}
	else {
		Status = STATUS_NOT_SUPPORTED;
	}

SetInformationEnd:
	WdfWaitLockRelease(DevExt->StateLock);
	Trace(TRACE_LEVEL_INFORMATION, SURFACE_BATTERY_TRACE,
		"Leaving " __FUNCTION__ ": Status=0x%x\n",
		Status);
	return Status;
}

_Use_decl_annotations_
NTSTATUS
SurfaceBatterySetBatteryString(
	PCWSTR String,
	PWCHAR Destination
)

/*++

Routine Description:

	Set one of the simulated battery strings.

Arguments:

	String - Supplies the new string value to set.

	Destination - Supplies a pointer to the buffer to store the new string.

Return Value:

   NTSTATUS

--*/

{

	PAGED_CODE();

	return RtlStringCchCopyW(Destination, MAX_BATTERY_STRING_SIZE, String);
}