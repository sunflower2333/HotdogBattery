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
#include "Spb.h"
#include "miniclass.tmh"

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

	Trace(TRACE_LEVEL_INFORMATION, SURFACE_BATTERY_TRACE, "Entering %!FUNC!\n");
	PAGED_CODE();

	DevExt = GetDeviceExtension(Device);

	WdfWaitLockAcquire(DevExt->StateLock, NULL);
	SurfaceBatteryUpdateTag(DevExt);
	WdfWaitLockRelease(DevExt->StateLock);

	Trace(TRACE_LEVEL_INFORMATION, SURFACE_BATTERY_TRACE,
		"Leaving %!FUNC!: Status = 0x%08lX\n",
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

	Trace(TRACE_LEVEL_INFORMATION, SURFACE_BATTERY_TRACE, "Entering %!FUNC!\n");
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
		"Leaving %!FUNC!: Status = 0x%08lX\n",
		Status);
	return Status;
}

NTSTATUS
SurfaceBatteryGetManufacturerBlockA(
	PSURFACE_BATTERY_FDO_DATA DevExt,
	PBQ27742_MANUF_INFO_TYPE ManufacturerBlockInfoA
)
{
	NTSTATUS Status;
	ULONG ResultValue;

	BYTE Data[32] = { 0 };
	
	LARGE_INTEGER delay = { 0 };

	Trace(TRACE_LEVEL_INFORMATION, SURFACE_BATTERY_TRACE, "Entering %!FUNC!\n");
	
	ResultValue = 0x01;
	Status = SpbWriteDataSynchronously(&DevExt->I2CContext, 0x3F, &ResultValue, 1);
	if (!NT_SUCCESS(Status))
	{
	    Trace(TRACE_LEVEL_ERROR, SURFACE_BATTERY_TRACE, "SpbWriteDataSynchronously failed with Status = 0x%08lX\n", Status);
		goto Exit;
	}

	delay.QuadPart = RELATIVE(MILLISECONDS(1));
	Status = KeDelayExecutionThread(KernelMode, TRUE, &delay);
	if (!NT_SUCCESS(Status))
	{
	    Trace(TRACE_LEVEL_ERROR, SURFACE_BATTERY_TRACE, "KeDelayExecutionThread failed with Status = 0x%08lX\n", Status);
		goto Exit;
	}

	Status = SpbReadDataSynchronously(&DevExt->I2CContext, 0x40, Data, sizeof(Data));
	if (!NT_SUCCESS(Status))
	{
	    Trace(TRACE_LEVEL_ERROR, SURFACE_BATTERY_TRACE, "SpbReadDataSynchronously failed with Status = 0x%08lX\n", Status);
		goto Exit;
	}

	RtlCopyMemory(ManufacturerBlockInfoA, Data, sizeof(BQ27742_MANUF_INFO_TYPE));

Exit:
	Trace(TRACE_LEVEL_INFORMATION, SURFACE_BATTERY_TRACE,
		"Leaving %!FUNC!: Status = 0x%08lX\n",
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
	
	BATTERY_REPORTING_SCALE ReportingScale = { 0 };
	BATTERY_INFORMATION BatteryInformationResult = { 0 };
	WCHAR StringResult[MAX_BATTERY_STRING_SIZE] = { 0 };
	BATTERY_MANUFACTURE_DATE ManufactureDate = { 0 };

	BQ27742_MANUF_INFO_TYPE ManufacturerBlockInfoA = { 0 };

	ULONG Temperature = 0;
	UCHAR Flags = 0;

	UNREFERENCED_PARAMETER(AtRate);

	Trace(TRACE_LEVEL_INFORMATION, SURFACE_BATTERY_TRACE, "Entering %!FUNC!\n");
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
		BatteryInformationResult.Capabilities = BATTERY_SYSTEM_BATTERY | BATTERY_CAPACITY_RELATIVE;
		BatteryInformationResult.Technology = 1;

		Status = SurfaceBatteryGetManufacturerBlockA(DevExt, &ManufacturerBlockInfoA);
		if (!NT_SUCCESS(Status))
		{
			Trace(TRACE_LEVEL_ERROR, SURFACE_BATTERY_TRACE, "SurfaceBatteryGetManufacturerBlockA failed with Status = 0x%08lX\n", Status);
			goto QueryInformationEnd;
		}
		
		RtlCopyMemory(BatteryInformationResult.Chemistry, ManufacturerBlockInfoA.Chemistry, sizeof(BatteryInformationResult.Chemistry));
		
		Status = SpbReadDataSynchronously(&DevExt->I2CContext, 0x3C, &BatteryInformationResult.DesignedCapacity, 2);
		if (!NT_SUCCESS(Status))
		{
			goto QueryInformationEnd;
		}
		
		Status = SpbReadDataSynchronously(&DevExt->I2CContext, 0x12, &BatteryInformationResult.FullChargedCapacity, 2);
		if (!NT_SUCCESS(Status))
		{
			Trace(TRACE_LEVEL_ERROR, SURFACE_BATTERY_TRACE, "SpbReadDataSynchronously failed with Status = 0x%08lX\n", Status);
			goto QueryInformationEnd;
		}

		BatteryInformationResult.DefaultAlert1 = 75;
		BatteryInformationResult.DefaultAlert2 = 150;
		BatteryInformationResult.CriticalBias = 0;
		
		Status = SpbReadDataSynchronously(&DevExt->I2CContext, 0x2A, &BatteryInformationResult.CycleCount, 2);
		if (!NT_SUCCESS(Status))
		{
		    Trace(TRACE_LEVEL_ERROR, SURFACE_BATTERY_TRACE, "SpbReadDataSynchronously failed with Status = 0x%08lX\n", Status);
			goto QueryInformationEnd;
		}

		ReturnBuffer = &BatteryInformationResult;

		ReturnBufferLength = sizeof(BATTERY_INFORMATION);
		Status = STATUS_SUCCESS;
		break;

	case BatteryEstimatedTime:
		Status = SpbReadDataSynchronously(&DevExt->I2CContext, 0x0A, &Flags, 2);
		if (!NT_SUCCESS(Status))
		{
			Trace(TRACE_LEVEL_ERROR, SURFACE_BATTERY_TRACE, "SpbReadDataSynchronously failed with Status = 0x%08lX\n", Status);
			goto QueryInformationEnd;
		}

		if (Flags & (1 << 0) || Flags & (1 << 1))
		{
			Status = SpbReadDataSynchronously(&DevExt->I2CContext, 0x16, &ResultValue, 2);
			if (!NT_SUCCESS(Status))
			{
				Trace(TRACE_LEVEL_ERROR, SURFACE_BATTERY_TRACE, "SpbReadDataSynchronously failed with Status = 0x%08lX\n", Status);
				goto QueryInformationEnd;
			}

			if (ResultValue == 0xFFFF)
			{
				ResultValue = BATTERY_UNKNOWN_TIME;
			}
			else
			{
				ResultValue *= 60;
			}
		}
		else
		{
			ResultValue = BATTERY_UNKNOWN_TIME;
		}
		
		ReturnBuffer = &ResultValue;
		ReturnBufferLength = sizeof(ResultValue);
		Status = STATUS_SUCCESS;
		break;

	case BatteryUniqueID:
		Status = SurfaceBatteryGetManufacturerBlockA(DevExt, &ManufacturerBlockInfoA);
		if (!NT_SUCCESS(Status))
		{
			Trace(TRACE_LEVEL_ERROR, SURFACE_BATTERY_TRACE, "SurfaceBatteryGetManufacturerBlockA failed with Status = 0x%08lX\n", Status);
			goto QueryInformationEnd;
		}

		swprintf_s(StringResult, sizeof(StringResult) / sizeof(WCHAR), L"%S%S%I32d%I32d", ManufacturerBlockInfoA.BatteryManufactureName, ManufacturerBlockInfoA.BatteryDeviceName, ManufacturerBlockInfoA.BatteryManufactureDate, ManufacturerBlockInfoA.BatterySerialNumber);

		ReturnBuffer = StringResult;
		Status = RtlStringCbLengthW(StringResult,
			sizeof(StringResult),
			&ReturnBufferLength);

		ReturnBufferLength += sizeof(WCHAR);
		Status = STATUS_SUCCESS;
		break;

	case BatteryManufactureName:
		Status = SurfaceBatteryGetManufacturerBlockA(DevExt, &ManufacturerBlockInfoA);
		if (!NT_SUCCESS(Status))
		{
			Trace(TRACE_LEVEL_ERROR, SURFACE_BATTERY_TRACE, "SurfaceBatteryGetManufacturerBlockA failed with Status = 0x%08lX\n", Status);
			goto QueryInformationEnd;
		}

		swprintf_s(StringResult, sizeof(StringResult) / sizeof(WCHAR), L"%S", ManufacturerBlockInfoA.BatteryManufactureName);

		ReturnBuffer = StringResult;
		Status = RtlStringCbLengthW(StringResult,
			sizeof(StringResult),
			&ReturnBufferLength);

		ReturnBufferLength += sizeof(WCHAR);
		Status = STATUS_SUCCESS;
		break;

	case BatteryDeviceName:
		Status = SurfaceBatteryGetManufacturerBlockA(DevExt, &ManufacturerBlockInfoA);
		if (!NT_SUCCESS(Status))
		{
			Trace(TRACE_LEVEL_ERROR, SURFACE_BATTERY_TRACE, "SurfaceBatteryGetManufacturerBlockA failed with Status = 0x%08lX\n", Status);
			goto QueryInformationEnd;
		}

		swprintf_s(StringResult, sizeof(StringResult) / sizeof(WCHAR), L"%S", ManufacturerBlockInfoA.BatteryDeviceName);

		ReturnBuffer = StringResult;
		Status = RtlStringCbLengthW(StringResult,
			sizeof(StringResult),
			&ReturnBufferLength);

		ReturnBufferLength += sizeof(WCHAR);
		Status = STATUS_SUCCESS;
		break;

	case BatterySerialNumber:
		Status = SurfaceBatteryGetManufacturerBlockA(DevExt, &ManufacturerBlockInfoA);
		if (!NT_SUCCESS(Status))
		{
			Trace(TRACE_LEVEL_ERROR, SURFACE_BATTERY_TRACE, "SurfaceBatteryGetManufacturerBlockA failed with Status = 0x%08lX\n", Status);
			goto QueryInformationEnd;
		}
		
		swprintf_s(StringResult, sizeof(StringResult) / sizeof(WCHAR), L"%I32d", ManufacturerBlockInfoA.BatterySerialNumber);
		
		ReturnBuffer = StringResult;
		Status = RtlStringCbLengthW(StringResult,
			sizeof(StringResult),
			&ReturnBufferLength);

		ReturnBufferLength += sizeof(WCHAR);
		Status = STATUS_SUCCESS;
		break;

	case BatteryManufactureDate:
		Status = SurfaceBatteryGetManufacturerBlockA(DevExt, &ManufacturerBlockInfoA);
		if (!NT_SUCCESS(Status))
		{
			Trace(TRACE_LEVEL_ERROR, SURFACE_BATTERY_TRACE, "SurfaceBatteryGetManufacturerBlockA failed with Status = 0x%08lX\n", Status);
			goto QueryInformationEnd;
		}

		ManufactureDate.Year = ManufacturerBlockInfoA.BatteryManufactureDate;
		ReturnBuffer = &ManufactureDate;
		ReturnBufferLength = sizeof(BATTERY_MANUFACTURE_DATE);
		Status = STATUS_SUCCESS;
		break;

	case BatteryGranularityInformation:
		Status = SpbReadDataSynchronously(&DevExt->I2CContext, 0x12, &ReportingScale.Capacity, 2);
		if (!NT_SUCCESS(Status))
		{
			Trace(TRACE_LEVEL_ERROR, SURFACE_BATTERY_TRACE, "SpbReadDataSynchronously failed with Status = 0x%08lX\n", Status);
			goto QueryInformationEnd;
		}

		ReportingScale.Granularity = 1;
		
		ReturnBuffer = &ReportingScale;
		ReturnBufferLength = sizeof(BATTERY_REPORTING_SCALE);
		Status = STATUS_SUCCESS;
		break;

	case BatteryTemperature:
		Status = SpbReadDataSynchronously(&DevExt->I2CContext, 0x06, &Temperature, 2);
		if (!NT_SUCCESS(Status))
		{
			Trace(TRACE_LEVEL_ERROR, SURFACE_BATTERY_TRACE, "SpbReadDataSynchronously failed with Status = 0x%08lX\n", Status);
			goto QueryInformationEnd;
		}

		ReturnBuffer = &Temperature;
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
		"Leaving %!FUNC!: Status = 0x%08lX\n",
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

	Trace(TRACE_LEVEL_INFORMATION, SURFACE_BATTERY_TRACE, "Entering %!FUNC!\n");
	PAGED_CODE();

	DevExt = (PSURFACE_BATTERY_FDO_DATA)Context;
	WdfWaitLockAcquire(DevExt->StateLock, NULL);
	if (BatteryTag != DevExt->BatteryTag) {
		Status = STATUS_NO_SUCH_DEVICE;
		goto QueryStatusEnd;
	}

	UCHAR Flags = 0;
	Status = SpbReadDataSynchronously(&DevExt->I2CContext, 0x0A, &Flags, 2);
	if (!NT_SUCCESS(Status))
	{
		Trace(TRACE_LEVEL_ERROR, SURFACE_BATTERY_TRACE, "SpbReadDataSynchronously failed with Status = 0x%08lX\n", Status);
		goto QueryStatusEnd;
	}
	
	if (Flags & (1 << 9))
	{
		BatteryStatus->PowerState = BATTERY_POWER_ON_LINE;
	}
	else if (Flags & (1 << 0))
	{
		BatteryStatus->PowerState = BATTERY_DISCHARGING;
	}
	else if (Flags & (1 << 1))
	{
		BatteryStatus->PowerState = BATTERY_CRITICAL;
	}
	else
	{
		BatteryStatus->PowerState = BATTERY_CHARGING;
	}
	
	Status = SpbReadDataSynchronously(&DevExt->I2CContext, 0x10, &BatteryStatus->Capacity, 2);
	if (!NT_SUCCESS(Status))
	{
		Trace(TRACE_LEVEL_ERROR, SURFACE_BATTERY_TRACE, "SpbReadDataSynchronously failed with Status = 0x%08lX\n", Status);
		goto QueryStatusEnd;
	}
	
	Status = SpbReadDataSynchronously(&DevExt->I2CContext, 0x08, &BatteryStatus->Voltage, 2);
	if (!NT_SUCCESS(Status))
	{
		Trace(TRACE_LEVEL_ERROR, SURFACE_BATTERY_TRACE, "SpbReadDataSynchronously failed with Status = 0x%08lX\n", Status);
		goto QueryStatusEnd;
	}
	
	Status = SpbReadDataSynchronously(&DevExt->I2CContext, 0x14, &BatteryStatus->Rate, 2);
	if (!NT_SUCCESS(Status))
	{
		Trace(TRACE_LEVEL_ERROR, SURFACE_BATTERY_TRACE, "SpbReadDataSynchronously failed with Status = 0x%08lX\n", Status);
		goto QueryStatusEnd;
	}

	Status = STATUS_SUCCESS;

QueryStatusEnd:
	WdfWaitLockRelease(DevExt->StateLock);
	Trace(TRACE_LEVEL_INFORMATION, SURFACE_BATTERY_TRACE,
		"Leaving %!FUNC!: Status = 0x%08lX\n",
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

	Trace(TRACE_LEVEL_INFORMATION, SURFACE_BATTERY_TRACE, "Entering %!FUNC!\n");
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
		"Leaving %!FUNC!: Status = 0x%08lX\n",
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

	Trace(TRACE_LEVEL_INFORMATION, SURFACE_BATTERY_TRACE, "Entering %!FUNC!\n");
	PAGED_CODE();

	Status = STATUS_NOT_SUPPORTED;
	Trace(TRACE_LEVEL_INFORMATION, SURFACE_BATTERY_TRACE,
		"Leaving %!FUNC!: Status = 0x%08lX\n",
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

	Trace(TRACE_LEVEL_INFORMATION, SURFACE_BATTERY_TRACE, "Entering %!FUNC!\n");
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
		//DevExt->State.MaxCurrentDraw = ChargingSource->MaxCurrent;
		Trace(TRACE_LEVEL_INFORMATION, SURFACE_BATTERY_INFO,
			"SurfaceBattery : Set MaxCurrentDraw = %u mA\n",
			ChargingSource->MaxCurrent);

		Status = STATUS_SUCCESS;
	}
	else {
		Status = STATUS_NOT_SUPPORTED;
	}

SetInformationEnd:
	WdfWaitLockRelease(DevExt->StateLock);
	Trace(TRACE_LEVEL_INFORMATION, SURFACE_BATTERY_TRACE,
		"Leaving %!FUNC!: Status = 0x%08lX\n",
		Status);
	return Status;
}