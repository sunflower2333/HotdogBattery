/*++

Copyright (c) Microsoft Corporation. All rights reserved.

Module Name:

	miniclass.c

Abstract:

	This module implements battery miniclass functionality specific to the
	Hotdog battery driver.

	N.B. This code is provided "AS IS" without any expressed or implied warranty.

--*/
// TODO: FIX the Charge Count.

//--------------------------------------------------------------------- Includes

#include "HotdogBattery.h"
#include "Spb.h"
#include "usbfnbase.h"
#include "miniclass.tmh"

//------------------------------------------------------------------- Prototypes

#define HotdogBatteryConvertToWatts(Value) ((Value) * 3870) / 1000

_IRQL_requires_same_
VOID
HotdogBatteryUpdateTag(
	_Inout_ PSURFACE_BATTERY_FDO_DATA DevExt
);

BCLASS_QUERY_TAG_CALLBACK HotdogBatteryQueryTag;
BCLASS_QUERY_INFORMATION_CALLBACK HotdogBatteryQueryInformation;
BCLASS_SET_INFORMATION_CALLBACK HotdogBatterySetInformation;
BCLASS_QUERY_STATUS_CALLBACK HotdogBatteryQueryStatus;
BCLASS_SET_STATUS_NOTIFY_CALLBACK HotdogBatterySetStatusNotify;
BCLASS_DISABLE_STATUS_NOTIFY_CALLBACK HotdogBatteryDisableStatusNotify;

//---------------------------------------------------------------------- Pragmas

#pragma alloc_text(PAGE, HotdogBatteryPrepareHardware)
#pragma alloc_text(PAGE, HotdogBatteryUpdateTag)
#pragma alloc_text(PAGE, HotdogBatteryQueryTag)
#pragma alloc_text(PAGE, HotdogBatteryQueryInformation)
#pragma alloc_text(PAGE, HotdogBatteryQueryStatus)
#pragma alloc_text(PAGE, HotdogBatterySetStatusNotify)
#pragma alloc_text(PAGE, HotdogBatteryDisableStatusNotify)
#pragma alloc_text(PAGE, HotdogBatterySetInformation)

//------------------------------------------------------------ Battery Interface
_Use_decl_annotations_
VOID
HotdogBatteryPrepareHardware(
	WDFDEVICE Device
)

/*++

Routine Description:

	This routine is called to initialize battery data to sane values.

Arguments:

	Device - Supplies the device to initialize.

Return Value:

	NTSTATUS

--*/

{

	PSURFACE_BATTERY_FDO_DATA DevExt;
	NTSTATUS Status = STATUS_SUCCESS;

	PAGED_CODE();
	Trace(TRACE_LEVEL_INFORMATION, SURFACE_BATTERY_TRACE, "Entering %!FUNC!\n");

	DevExt = GetDeviceExtension(Device);

	WdfWaitLockAcquire(DevExt->StateLock, NULL);
	HotdogBatteryUpdateTag(DevExt);
	WdfWaitLockRelease(DevExt->StateLock);

	Trace(TRACE_LEVEL_INFORMATION, SURFACE_BATTERY_TRACE,
		"Leaving %!FUNC!: Status = 0x%08lX\n",
		Status);
	return;
}

_Use_decl_annotations_
VOID
HotdogBatteryUpdateTag(
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
HotdogBatteryQueryTag(
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

	PAGED_CODE();
	Trace(TRACE_LEVEL_INFORMATION, SURFACE_BATTERY_TRACE, "Entering %!FUNC!\n");

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
HotdogBatteryQueryBatteryInformation(
	PSURFACE_BATTERY_FDO_DATA DevExt,
	PBATTERY_INFORMATION BatteryInformationResult
)
{
	NTSTATUS Status;
	Trace(TRACE_LEVEL_INFORMATION, SURFACE_BATTERY_TRACE, "Entering %!FUNC!\n");

	BatteryInformationResult->Capabilities =
		BATTERY_SYSTEM_BATTERY |
		BATTERY_SET_CHARGE_SUPPORTED |
		BATTERY_SET_DISCHARGE_SUPPORTED |
		BATTERY_SET_CHARGINGSOURCE_SUPPORTED |
		BATTERY_SET_CHARGER_ID_SUPPORTED;
	// BATTERY_CAPACITY_RELATIVE |
	BatteryInformationResult->Technology = 1;

	BYTE LION[4] = {'L','I','O','N'};
	RtlCopyMemory(BatteryInformationResult->Chemistry, LION, 4);
	Status = SpbReadDataSynchronously(&DevExt->I2CContext, 0x3C, &BatteryInformationResult->DesignedCapacity, 2);
	if (!NT_SUCCESS(Status))
	{
		Trace(TRACE_LEVEL_ERROR, SURFACE_BATTERY_TRACE, "SpbReadDataSynchronously failed with Status = 0x%08lX\n", Status);
		goto Exit;
	}

	BatteryInformationResult->DesignedCapacity = HotdogBatteryConvertToWatts(BatteryInformationResult->DesignedCapacity);
	
	Status = SpbReadDataSynchronously(&DevExt->I2CContext, 0x0A, &BatteryInformationResult->FullChargedCapacity, 2);
	if (!NT_SUCCESS(Status))
	{
		Trace(TRACE_LEVEL_ERROR, SURFACE_BATTERY_TRACE, "SpbReadDataSynchronously failed with Status = 0x%08lX\n", Status);
		goto Exit;
	}
	Trace(TRACE_LEVEL_INFORMATION, SURFACE_BATTERY_TRACE, "FullChargedCapacity 0x13: %x", BatteryInformationResult->FullChargedCapacity);

	Trace(TRACE_LEVEL_INFORMATION, SURFACE_BATTERY_TRACE, "FullChargedCapacity BeforeTransfer 0x12: %x", BatteryInformationResult->FullChargedCapacity);
	BatteryInformationResult->FullChargedCapacity = HotdogBatteryConvertToWatts(BatteryInformationResult->FullChargedCapacity);
	Trace(TRACE_LEVEL_INFORMATION, SURFACE_BATTERY_TRACE, "FullChargedCapacity AfterTransfer: %x", BatteryInformationResult->FullChargedCapacity);

	BatteryInformationResult->DefaultAlert1 = BatteryInformationResult->FullChargedCapacity * 7 / 100; // 7% of total capacity for error
	BatteryInformationResult->DefaultAlert2 = BatteryInformationResult->FullChargedCapacity * 9 / 100; // 9% of total capacity for warning
	BatteryInformationResult->CriticalBias = 0;

	Status = SpbReadDataSynchronously(&DevExt->I2CContext, 0x2A, &BatteryInformationResult->CycleCount, 2);
	if (!NT_SUCCESS(Status))
	{
		Trace(TRACE_LEVEL_ERROR, SURFACE_BATTERY_TRACE, "SpbReadDataSynchronously failed with Status = 0x%08lX\n", Status);
		goto Exit;
	}

	Trace(
		TRACE_LEVEL_INFORMATION,
		SURFACE_BATTERY_TRACE,
		"BATTERY_INFORMATION: \n"
		"Capabilities: %d \n"
		"Technology: %d \n"
		"Chemistry: %c%c%c%c \n"
		"DesignedCapacity: %d \n"
		"FullChargedCapacity: %d \n"
		"DefaultAlert1: %d \n"
		"DefaultAlert2: %d \n"
		"CriticalBias: %d \n"
		"CycleCount: %d\n",
		BatteryInformationResult->Capabilities,
		BatteryInformationResult->Technology,
		BatteryInformationResult->Chemistry[0],
		BatteryInformationResult->Chemistry[1],
		BatteryInformationResult->Chemistry[2],
		BatteryInformationResult->Chemistry[3],
		BatteryInformationResult->DesignedCapacity,
		BatteryInformationResult->FullChargedCapacity,
		BatteryInformationResult->DefaultAlert1,
		BatteryInformationResult->DefaultAlert2,
		BatteryInformationResult->CriticalBias,
		BatteryInformationResult->CycleCount);

Exit:
	Trace(TRACE_LEVEL_INFORMATION, SURFACE_BATTERY_TRACE,
		"Leaving %!FUNC!: Status = 0x%08lX\n",
		Status);
	return Status;
}

NTSTATUS
HotdogBatteryQueryBatteryEstimatedTime(
	PSURFACE_BATTERY_FDO_DATA DevExt,
	LONG AtRate,
	PULONG ResultValue
)
{
	NTSTATUS Status = STATUS_SUCCESS;
	UCHAR Flags = 0;
	UINT16 ETA = 0;

	Trace(TRACE_LEVEL_INFORMATION, SURFACE_BATTERY_TRACE, "Entering %!FUNC!\n");

	if (AtRate == 0)
	{
		Status = SpbReadDataSynchronously(&DevExt->I2CContext, 0x06, &Flags, 2);
		if (!NT_SUCCESS(Status))
		{
			Trace(TRACE_LEVEL_ERROR, SURFACE_BATTERY_TRACE, "SpbReadDataSynchronously failed with Status = 0x%08lX\n", Status);
			goto Exit;
		}

		if (Flags & (1 << 0) || Flags & (1 << 1))
		{
			Status = SpbReadDataSynchronously(&DevExt->I2CContext, 0x0C, &ETA, 2);
			if (!NT_SUCCESS(Status))
			{
				Trace(TRACE_LEVEL_ERROR, SURFACE_BATTERY_TRACE, "SpbReadDataSynchronously failed with Status = 0x%08lX\n", Status);
				goto Exit;
			}

			if (ETA == 0xFFFF)
			{
				*ResultValue = BATTERY_UNKNOWN_TIME;

				Trace(
					TRACE_LEVEL_INFORMATION,
					SURFACE_BATTERY_TRACE,
					"BatteryEstimatedTime: BATTERY_UNKNOWN_TIME\n");
			}
			else
			{
				*ResultValue = ETA * 60;

				Trace(
					TRACE_LEVEL_INFORMATION,
					SURFACE_BATTERY_TRACE,
					"BatteryEstimatedTime: %d seconds\n",
					*ResultValue);
			}
		}
		else
		{
			*ResultValue = BATTERY_UNKNOWN_TIME;

			Trace(
				TRACE_LEVEL_INFORMATION,
				SURFACE_BATTERY_TRACE,
				"BatteryEstimatedTime: BATTERY_UNKNOWN_TIME\n");
		}
	}
	else
	{
		*ResultValue = BATTERY_UNKNOWN_TIME;

		Trace(
			TRACE_LEVEL_INFORMATION,
			SURFACE_BATTERY_TRACE,
			"BatteryEstimatedTime: BATTERY_UNKNOWN_TIME for AtRate = %d\n",
			AtRate);
	}

Exit:
	Trace(TRACE_LEVEL_INFORMATION, SURFACE_BATTERY_TRACE,
		"Leaving %!FUNC!: Status = 0x%08lX\n",
		Status);
	return Status;
}

_Use_decl_annotations_
NTSTATUS
HotdogBatteryQueryInformation(
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

	ULONG Temperature = 0;

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
	//

	ReturnBuffer = NULL;
	ReturnBufferLength = 0;
	Trace(TRACE_LEVEL_INFORMATION, SURFACE_BATTERY_INFO, "Query for information level 0x%x\n", Level);
	Status = STATUS_INVALID_DEVICE_REQUEST;
	switch (Level) {
	case BatteryInformation:
		Status = HotdogBatteryQueryBatteryInformation(DevExt, &BatteryInformationResult);
		if (!NT_SUCCESS(Status))
		{
			Trace(TRACE_LEVEL_ERROR, SURFACE_BATTERY_TRACE, "HotdogBatteryQueryBatteryInformation failed with Status = 0x%08lX\n", Status);
			goto Exit;
		}

		ReturnBuffer = &BatteryInformationResult;
		ReturnBufferLength = sizeof(BATTERY_INFORMATION);
		Status = STATUS_SUCCESS;
		break;

	case BatteryEstimatedTime:
		Status = HotdogBatteryQueryBatteryEstimatedTime(DevExt, AtRate, &ResultValue);
		if (!NT_SUCCESS(Status))
		{
			Trace(TRACE_LEVEL_ERROR, SURFACE_BATTERY_TRACE, "HotdogBatteryQueryBatteryEstimatedTime failed with Status = 0x%08lX\n", Status);
			goto Exit;
		}

		ReturnBuffer = &ResultValue;
		ReturnBufferLength = sizeof(ResultValue);
		Status = STATUS_SUCCESS;
		break;

	case BatteryUniqueID:
		//Status = HotdogBatteryGetManufacturerBlockA(DevExt, &ManufacturerBlockInfoA);
		//if (!NT_SUCCESS(Status))
		//{
		//	Trace(TRACE_LEVEL_ERROR, SURFACE_BATTERY_TRACE, "HotdogBatteryGetManufacturerBlockA failed with Status = 0x%08lX\n", Status);
		//	goto Exit;
		//}

		swprintf_s(StringResult, sizeof(StringResult) / sizeof(WCHAR), L"%c%c%c%c%c%c%c%c%c%c%c%c%u",
			'O',
			'P',
			'7',
			'P',
			'P',
			'B',
			'A',
			'T',
			'T',
			'E',
			'R',
			'Y',
			2333);

		Trace(
			TRACE_LEVEL_INFORMATION,
			SURFACE_BATTERY_TRACE,
			"BatteryUniqueID: %S\n",
			StringResult);

		Status = RtlStringCbLengthW(StringResult,
			sizeof(StringResult),
			&ReturnBufferLength);
		if (!NT_SUCCESS(Status))
		{
			Trace(TRACE_LEVEL_ERROR, SURFACE_BATTERY_TRACE, "RtlStringCbLengthW failed with Status = 0x%08lX\n", Status);
			goto Exit;
		}

		ReturnBuffer = StringResult;
		ReturnBufferLength += sizeof(WCHAR);
		Status = STATUS_SUCCESS;
		break;

	case BatteryManufactureName:
		//Status = HotdogBatteryGetManufacturerBlockA(DevExt, &ManufacturerBlockInfoA);
		//if (!NT_SUCCESS(Status))
		//{
		//	Trace(TRACE_LEVEL_ERROR, SURFACE_BATTERY_TRACE, "HotdogBatteryGetManufacturerBlockA failed with Status = 0x%08lX\n", Status);
		//	goto Exit;
		//}

		swprintf_s(StringResult, sizeof(StringResult) / sizeof(WCHAR), L"%c%c",
			0x4f,  // O
			0x50   // P
		);

		Trace(
			TRACE_LEVEL_INFORMATION,
			SURFACE_BATTERY_TRACE,
			"BatteryManufactureName: %S\n",
			StringResult);

		Status = RtlStringCbLengthW(StringResult,
			sizeof(StringResult),
			&ReturnBufferLength);
		if (!NT_SUCCESS(Status))
		{
			Trace(TRACE_LEVEL_ERROR, SURFACE_BATTERY_TRACE, "RtlStringCbLengthW failed with Status = 0x%08lX\n", Status);
			goto Exit;
		}

		ReturnBuffer = StringResult;
		ReturnBufferLength += sizeof(WCHAR);
		Status = STATUS_SUCCESS;
		break;

	case BatteryDeviceName:
		//Status = HotdogBatteryGetManufacturerBlockA(DevExt, &ManufacturerBlockInfoA);
		//if (!NT_SUCCESS(Status))
		//{
		//	Trace(TRACE_LEVEL_ERROR, SURFACE_BATTERY_TRACE, "HotdogBatteryGetManufacturerBlockA failed with Status = 0x%08lX\n", Status);
		//	goto Exit;
		//}

		swprintf_s(StringResult, sizeof(StringResult) / sizeof(WCHAR), L"%c%c%c%c%c%c",
			0x42,  //B
			0x4c,  //L
			0x50,  //P
			0x37,  //7
			0x34,  //4
			0x35   //5
		);

		Trace(
			TRACE_LEVEL_INFORMATION,
			SURFACE_BATTERY_TRACE,
			"BatteryDeviceName: %S\n",
			StringResult);

		Status = RtlStringCbLengthW(StringResult,
			sizeof(StringResult),
			&ReturnBufferLength);
		if (!NT_SUCCESS(Status))
		{
			Trace(TRACE_LEVEL_ERROR, SURFACE_BATTERY_TRACE, "RtlStringCbLengthW failed with Status = 0x%08lX\n", Status);
			goto Exit;
		}

		ReturnBuffer = StringResult;
		ReturnBufferLength += sizeof(WCHAR);
		Status = STATUS_SUCCESS;
		break;

	case BatterySerialNumber:
		//Status = HotdogBatteryGetManufacturerBlockA(DevExt, &ManufacturerBlockInfoA);
		//if (!NT_SUCCESS(Status))
		//{
		//	Trace(TRACE_LEVEL_ERROR, SURFACE_BATTERY_TRACE, "HotdogBatteryGetManufacturerBlockA failed with Status = 0x%08lX\n", Status);
		//	goto Exit;
		//}

		swprintf_s(StringResult, sizeof(StringResult) / sizeof(WCHAR), L"%u", (UINT32)2333);

		Trace(
			TRACE_LEVEL_INFORMATION,
			SURFACE_BATTERY_TRACE,
			"BatterySerialNumber: %S\n",
			StringResult);

		Status = RtlStringCbLengthW(StringResult,
			sizeof(StringResult),
			&ReturnBufferLength);
		if (!NT_SUCCESS(Status))
		{
			Trace(TRACE_LEVEL_ERROR, SURFACE_BATTERY_TRACE, "RtlStringCbLengthW failed with Status = 0x%08lX\n", Status);
			goto Exit;
		}

		ReturnBuffer = StringResult;
		ReturnBufferLength += sizeof(WCHAR);
		Status = STATUS_SUCCESS;
		break;

	case BatteryManufactureDate:
		//Status = HotdogBatteryGetManufacturerBlockA(DevExt, &ManufacturerBlockInfoA);
		//if (!NT_SUCCESS(Status))
		//{
		//	Trace(TRACE_LEVEL_ERROR, SURFACE_BATTERY_TRACE, "HotdogBatteryGetManufacturerBlockA failed with Status = 0x%08lX\n", Status);
		//	goto Exit;
		//}

		//Trace(
		//	TRACE_LEVEL_INFORMATION,
		//	SURFACE_BATTERY_TRACE,
		//	"BatteryManufactureDate: %d\n",
		//	ManufacturerBlockInfoA.BatteryManufactureDate);

		////ManufactureDate.Year = ManufacturerBlockInfoA.BatteryManufactureDate;
		ManufactureDate.Day = 1;
		ManufactureDate.Month = 1;
		ManufactureDate.Year = 2019;

		ReturnBuffer = &ManufactureDate;
		ReturnBufferLength = sizeof(BATTERY_MANUFACTURE_DATE);
		Status = STATUS_SUCCESS;
		break;

	case BatteryGranularityInformation:
		Status = SpbReadDataSynchronously(&DevExt->I2CContext, 0x0A, &ReportingScale.Capacity, 2);
		if (!NT_SUCCESS(Status))
		{
			Trace(TRACE_LEVEL_ERROR, SURFACE_BATTERY_TRACE, "SpbReadDataSynchronously failed with Status = 0x%08lX\n", Status);
			goto Exit;
		}

		ReportingScale.Capacity = HotdogBatteryConvertToWatts(ReportingScale.Capacity);
		ReportingScale.Granularity = 1;

		Trace(
			TRACE_LEVEL_INFORMATION,
			SURFACE_BATTERY_TRACE,
			"BATTERY_REPORTING_SCALE: Capacity: %d, Granularity: %d\n",
			ReportingScale.Capacity,
			ReportingScale.Granularity);

		ReturnBuffer = &ReportingScale;
		ReturnBufferLength = sizeof(BATTERY_REPORTING_SCALE);
		Status = STATUS_SUCCESS;
		break;

	case BatteryTemperature:
		Status = SpbReadDataSynchronously(&DevExt->I2CContext, 0x02, &Temperature, 2);
		if (!NT_SUCCESS(Status))
		{
			Trace(TRACE_LEVEL_ERROR, SURFACE_BATTERY_TRACE, "SpbReadDataSynchronously failed with Status = 0x%08lX\n", Status);
			goto Exit;
		}

		Trace(
			TRACE_LEVEL_INFORMATION,
			SURFACE_BATTERY_TRACE,
			"BatteryTemperature: %d\n",
			Temperature);

		ReturnBuffer = &Temperature;
		ReturnBufferLength = sizeof(ULONG);
		Status = STATUS_SUCCESS;
		break;

	default:
		Status = STATUS_INVALID_PARAMETER;
		break;
	}

Exit:
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
struct BQ27541_SOC_DATA {
	UINT32 unkownDATA;
	UINT16 SOC;
};

_Use_decl_annotations_
NTSTATUS
HotdogBatteryQueryStatus(
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
	INT16 Rate = 0;
	UCHAR Flags = 0;

	Trace(TRACE_LEVEL_INFORMATION, SURFACE_BATTERY_TRACE, "Entering %!FUNC!\n");
	PAGED_CODE();

	DevExt = (PSURFACE_BATTERY_FDO_DATA)Context;
	WdfWaitLockAcquire(DevExt->StateLock, NULL);
	if (BatteryTag != DevExt->BatteryTag) {
		Status = STATUS_NO_SUCH_DEVICE;
		goto QueryStatusEnd;
	}

	Status = SpbReadDataSynchronously(&DevExt->I2CContext, 0x06, &Flags, 2);
	if (!NT_SUCCESS(Status))
	{
		Trace(TRACE_LEVEL_ERROR, SURFACE_BATTERY_TRACE, "SpbReadDataSynchronously failed with Status = 0x%08lX\n", Status);
		goto QueryStatusEnd;
	}

	if (Flags & (1 << 9))
	{
		Trace(
			TRACE_LEVEL_INFORMATION,
			SURFACE_BATTERY_TRACE,
			"BATTERY_POWER_ON_LINE\n");

		BatteryStatus->PowerState = BATTERY_POWER_ON_LINE;
	}
	else if (Flags & (1 << 0))
	{
		Trace(
			TRACE_LEVEL_INFORMATION,
			SURFACE_BATTERY_TRACE,
			"BATTERY_DISCHARGING\n");

		BatteryStatus->PowerState = BATTERY_DISCHARGING;
	}
	else if (Flags & (1 << 1))
	{
		Trace(
			TRACE_LEVEL_INFORMATION,
			SURFACE_BATTERY_TRACE,
			"BATTERY_CRITICAL\n");

		BatteryStatus->PowerState = BATTERY_CRITICAL;
	}
	else
	{
		Trace(
			TRACE_LEVEL_INFORMATION,
			SURFACE_BATTERY_TRACE,
			"BATTERY_CHARGING\n");

		BatteryStatus->PowerState = BATTERY_CHARGING;
	}

	Status = SpbReadDataSynchronously(&DevExt->I2CContext, 0x08, &BatteryStatus->Capacity, 2);
	if (!NT_SUCCESS(Status))
	{
		Trace(TRACE_LEVEL_ERROR, SURFACE_BATTERY_TRACE, "SpbReadDataSynchronously failed with Status = 0x%08lX\n", Status);
		goto QueryStatusEnd;
	}
	BatteryStatus->Capacity = HotdogBatteryConvertToWatts(BatteryStatus->Capacity);

	Status = SpbReadDataSynchronously(&DevExt->I2CContext, 0x04, &BatteryStatus->Voltage, 2);
	if (!NT_SUCCESS(Status))
	{
		Trace(TRACE_LEVEL_ERROR, SURFACE_BATTERY_TRACE, "SpbReadDataSynchronously failed with Status = 0x%08lX\n", Status);
		goto QueryStatusEnd;
	}

	Status = SpbReadDataSynchronously(&DevExt->I2CContext, 0x10, &Rate, 2);
	if (!NT_SUCCESS(Status))
	{
		Trace(TRACE_LEVEL_ERROR, SURFACE_BATTERY_TRACE, "SpbReadDataSynchronously failed with Status = 0x%08lX\n", Status);
		goto QueryStatusEnd;
	}

	BatteryStatus->Rate = Rate;

	BatteryStatus->Rate = HotdogBatteryConvertToWatts(BatteryStatus->Rate);

	Trace(
		TRACE_LEVEL_INFORMATION,
		SURFACE_BATTERY_TRACE,
		"BATTERY_STATUS: \n"
		"PowerState: %d \n"
		"Capacity: %d \n"
		"Voltage: %d \n"
		"Rate: %d\n",
		BatteryStatus->PowerState,
		BatteryStatus->Capacity,
		BatteryStatus->Voltage,
		BatteryStatus->Rate);

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
HotdogBatterySetStatusNotify(
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
HotdogBatteryDisableStatusNotify(
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
HotdogBatterySetInformation(
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
	PULONG CriticalBias;
	PBATTERY_CHARGER_ID ChargerId;
	PBATTERY_CHARGER_STATUS ChargerStatus;
	//PBATTERY_USB_CHARGER_STATUS UsbChargerStatus;
	//PUSBFN_PORT_TYPE UsbFnPortType;
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

	if (Level == BatteryCharge)
	{
		Trace(TRACE_LEVEL_INFORMATION, SURFACE_BATTERY_INFO,
			"HotdogBattery : BatteryCharge\n");

		Status = STATUS_SUCCESS;
	}
	else if (Level == BatteryDischarge)
	{
		Trace(TRACE_LEVEL_INFORMATION, SURFACE_BATTERY_INFO,
			"HotdogBattery : BatteryDischarge\n");

		Status = STATUS_SUCCESS;
	}
	else if (Buffer == NULL)
	{
		Status = STATUS_INVALID_PARAMETER_4;
	}
	else if (Level == BatteryChargingSource)
	{
		ChargingSource = (PBATTERY_CHARGING_SOURCE)Buffer;

		Trace(TRACE_LEVEL_INFORMATION, SURFACE_BATTERY_INFO,
			"HotdogBattery : BatteryChargingSource Type = %d\n",
			ChargingSource->Type);

		Trace(TRACE_LEVEL_INFORMATION, SURFACE_BATTERY_INFO,
			"HotdogBattery : Set MaxCurrentDraw = %u mA\n",
			ChargingSource->MaxCurrent);

		Status = STATUS_SUCCESS;
	}
	else if (Level == BatteryCriticalBias)
	{
		CriticalBias = (PULONG)Buffer;
		Trace(TRACE_LEVEL_INFORMATION, SURFACE_BATTERY_INFO,
			"HotdogBattery : Set CriticalBias = %u mW\n",
			*CriticalBias);

		Status = STATUS_SUCCESS;
	}
	else if (Level == BatteryChargerId)
	{
		ChargerId = (PBATTERY_CHARGER_ID)Buffer;
		Trace(TRACE_LEVEL_INFORMATION, SURFACE_BATTERY_INFO,
			"HotdogBattery : BatteryChargerId = %!GUID!\n",
			ChargerId);

		Status = STATUS_SUCCESS;
	}
	else if (Level == BatteryChargerStatus)
	{
		ChargerStatus = (PBATTERY_CHARGER_STATUS)Buffer;

		Trace(TRACE_LEVEL_INFORMATION, SURFACE_BATTERY_INFO,
			"HotdogBattery : BatteryChargingSource Type = %d\n",
			ChargerStatus->Type);

		/*if (ChargerStatus->Type == BatteryChargingSourceType_USB)
		{
			UsbChargerStatus = (PBATTERY_USB_CHARGER_STATUS)Buffer;

			Trace(TRACE_LEVEL_INFORMATION, SURFACE_BATTERY_INFO,
				"HotdogBattery : BatteryChargingSourceType_USB: Flags = %d, MaxCurrent = %d, Voltage = %d, PortType = %d, PortId = %llu, OemCharger = %!GUID!\n",
				UsbChargerStatus->Flags, UsbChargerStatus->MaxCurrent, UsbChargerStatus->Voltage, UsbChargerStatus->PortType, UsbChargerStatus->PortId, &UsbChargerStatus->OemCharger);

			UsbFnPortType = (PUSBFN_PORT_TYPE)UsbChargerStatus->PowerSourceInformation;

			Trace(TRACE_LEVEL_INFORMATION, SURFACE_BATTERY_INFO,
				"HotdogBattery : UsbFnPortType = %d\n",
				*UsbFnPortType);
		}*/

		Status = STATUS_SUCCESS;
	}
	else
	{
		Status = STATUS_NOT_SUPPORTED;
	}

SetInformationEnd:
	WdfWaitLockRelease(DevExt->StateLock);
	Trace(TRACE_LEVEL_INFORMATION, SURFACE_BATTERY_TRACE,
		"Leaving %!FUNC!: Status = 0x%08lX\n",
		Status);
	return Status;
}