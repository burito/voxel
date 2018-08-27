/*
Copyright (c) 2014 Daniel Burke

This software is provided 'as-is', without any express or implied
warranty. In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

   1. The origin of this software must not be misrepresented; you must not
   claim that you wrote the original software. If you use this software
   in a product, an acknowledgment in the product documentation would be
   appreciated but is not required.

   2. Altered source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.

   3. This notice may not be removed or altered from any source
   distribution.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#define HMODULE void*
#endif

#include "log.h"

//#include "gpu/adl_sdk.h"


/*
	NVML related stuff.
	https://developer.nvidia.com/nvidia-management-library-nvml
*/
HMODULE nvml=NULL;

struct nvmlDevice_st;
typedef struct nvmlDevice_st* nvmlDevice_t;

typedef int (*NVI)(void);
NVI nvmlInit = NULL;
typedef int (*NVS)(void);
NVS nvmlShutdown = NULL;
typedef int (*NVDGC)(int*);
NVDGC nvmlDeviceGetCount = NULL;
typedef int (*NVDGHBI)(unsigned int, nvmlDevice_t*);
NVDGHBI nvmlDeviceGetHandleByIndex = NULL;
typedef int (*NVDGT)(nvmlDevice_t, int, int*);
NVDGT nvmlDeviceGetTemperature = NULL;

int nvml_device_count = 0;
int *nvml_gputemp=NULL;

nvmlDevice_t *nvml_devices;

/*
	ADL specific stuff
	http://developer.amd.com/tools-and-sdks/graphics-development/display-library-adl-sdk/
*/
HMODULE adl=NULL;
#define ADL_MAX_PATH 256

#define ADL_DL_THERMAL_DOMAIN_GPU		 1

typedef struct AdapterInfo
{
	int iSize;
	int iAdapterIndex;
	char strUDID[ADL_MAX_PATH];
	int iBusNumber;
	int iDeviceNumber;
	int iFunctionNumber;
	int iVendorID;
	char strAdapterName[ADL_MAX_PATH];
	char strDisplayName[ADL_MAX_PATH];
	int iPresent;
#if defined (_WIN32) || defined (_WIN64)
	int iExist;
	char strDriverPath[ADL_MAX_PATH];
	char strDriverPathExt[ADL_MAX_PATH];
	char strPNPString[ADL_MAX_PATH];
	int iOSDisplayIndex;
#endif /* (_WIN32) || (_WIN64) */
#if defined (LINUX)
	int iXScreenNum;
	int iDrvIndex;
	char strXScreenConfigName[ADL_MAX_PATH];
#endif /* (LINUX) */
} AdapterInfo, *LPAdapterInfo;

typedef struct ADLTemperature
{
	int iSize;
	int iTemperature;
} ADLTemperature;

typedef struct ADLThermalControllerInfo
{
	int iSize;
	int iThermalDomain;
	int iDomainIndex;
	int iFlags;
} ADLThermalControllerInfo;



typedef void* ( *ADL_MAIN_MALLOC_CALLBACK )( int );
// Memory allocation function
static void* ADL_Main_Memory_Alloc ( int iSize )
{
	void* lpBuffer = malloc ( iSize );
	return lpBuffer;
}

typedef int ( *ADL_MAIN_CONTROL_CREATE )(ADL_MAIN_MALLOC_CALLBACK, int );
typedef int ( *ADL_MAIN_CONTROL_DESTROY )();
typedef int ( *ADL_ADAPTER_NUMBEROFADAPTERS_GET ) ( int* );
typedef int ( *ADL_ADAPTER_ADAPTERINFO_GET ) ( LPAdapterInfo, int );
typedef int ( *ADL_OVERDRIVE_CAPS ) (int iAdapterIndex, int *iSupported, int *iEnabled, int *iVersion);
ADL_MAIN_CONTROL_CREATE					ADL_Main_Control_Create;
ADL_MAIN_CONTROL_DESTROY				ADL_Main_Control_Destroy;
ADL_ADAPTER_NUMBEROFADAPTERS_GET		ADL_Adapter_NumberOfAdapters_Get;
ADL_ADAPTER_ADAPTERINFO_GET				ADL_Adapter_AdapterInfo_Get;
ADL_OVERDRIVE_CAPS						ADL_Overdrive_Caps;

typedef int ( *ADL_OVERDRIVE5_THERMALDEVICES_ENUM ) (int iAdapterIndex, int iThermalControllerIndex, ADLThermalControllerInfo *lpThermalControllerInfo);
typedef int ( *ADL_OVERDRIVE5_TEMPERATURE_GET ) (int iAdapterIndex, int iThermalControllerIndex, ADLTemperature *lpTemperature);
ADL_OVERDRIVE5_TEMPERATURE_GET			ADL_Overdrive5_Temperature_Get;
ADL_OVERDRIVE5_THERMALDEVICES_ENUM		ADL_Overdrive5_ThermalDevices_Enum;

typedef int ( *ADL_OVERDRIVE6_TEMPERATURE_GET )(int iAdapterIndex, int *lpTemperature);
ADL_OVERDRIVE6_TEMPERATURE_GET ADL_Overdrive6_Temperature_Get;

int adl_device_count=0;
int *adl_gputemp=NULL;
int *adl_devid=NULL;
int *adl_odver=NULL;
int *adl_domain_id=NULL;

void gpuinfo_tick(void)
{

	if(nvml)
	{
		for(int i=0; i < nvml_device_count; i++)
		{
			int ret = nvmlDeviceGetTemperature(nvml_devices[i], 0,
				&nvml_gputemp[i]);
			if(ret)log_error("nvmlDeviceGetTemperature(%d) = %d", i, ret);
//			log_info("Temperature[%d] is %d", i, temp);
		}

	}

	if(adl)
	{
		int ret;
		ADLTemperature adlTmp = {sizeof(ADLTemperature), 0};
		for(int i=0; i < adl_device_count; i++)
		switch(adl_odver[i]) {
		case 6:
			ret = ADL_Overdrive6_Temperature_Get(
					adl_devid[i], &adl_gputemp[i]);
			adl_gputemp[i] /= 1000;
			if(ret)log_error("ADL_Overdrive6_Temperature_Get(%d) = %d", i, ret);
			break;
		case 5:
			adlTmp.iSize = sizeof(ADLTemperature);
			adlTmp.iTemperature = 0;
			ret = ADL_Overdrive5_Temperature_Get(
					adl_devid[i], adl_domain_id[i], &adlTmp);
			adl_gputemp[i] = adlTmp.iTemperature / 1000;
			if(ret)log_error("ADL_Overdrive5_Temperature_Get(%d) = %d", i, ret);

			break;
		default:
			break;
		}
	}
}


#ifdef _WIN32

#define EvilMacro(L, T, N, NS) \
	N = (T)GetProcAddress(L, NS); \
	if(!N)log_fatal("GetProcAddress(\"N\")")

#else

#define EvilMacro(L, T, N, NS) \
	*(void **)(&N) = dlsym(L, NS); \
	if(!N)log_fatal("dlsym(\"N\")")

#endif

void gpuinfo_init(void)
{
#ifdef _WIN32
	char nvmlpath[] = "/NVIDIA Corporation/NVSMI/nvml.dll";
	char dllpath[MAX_PATH];
	memset(dllpath, 0, MAX_PATH);
	GetEnvironmentVariable("ProgramFiles",
			dllpath, MAX_PATH-(strlen(nvmlpath)+1) );
	strcpy(dllpath + strlen(dllpath), nvmlpath);
	nvml = LoadLibrary(dllpath);
	adl = LoadLibrary("atiadlxx.dll");
	if(!adl) adl = LoadLibrary("atiadlxy.dll");	// for 32bit systems
#else
	nvml = dlopen("libnvidia-ml.so", RTLD_LAZY);
	adl = dlopen("libatiadlxx.so", RTLD_LAZY);
#endif

	log_info("GPU Temp   : %s %s", nvml? "NVML ": "", adl? "ADL": "");

	if(nvml)
	{
		EvilMacro(nvml, NVI, nvmlInit, "nvmlInit");
		EvilMacro(nvml, NVS, nvmlShutdown, "nvmlShutdown");
		EvilMacro(nvml, NVDGC, nvmlDeviceGetCount, "nvmlDeviceGetCount");
		EvilMacro(nvml, NVDGHBI, nvmlDeviceGetHandleByIndex,
			"nvmlDeviceGetHandleByIndex");
		EvilMacro(nvml, NVDGT, nvmlDeviceGetTemperature,
			"nvmlDeviceGetTemperature");

		int ret = nvmlInit();
		if(ret)log_error("nvmlInit() = %d", ret);

		ret = nvmlDeviceGetCount(&nvml_device_count);
		if(ret)log_error("nvmlDeviceGetCount() = %d", ret);

		nvml_devices = malloc(sizeof(nvmlDevice_t)*nvml_device_count);
		nvml_gputemp = malloc(sizeof(int)*nvml_device_count);
		for(int i=0; i < nvml_device_count; i++)
		{
			nvml_gputemp[i]=0;
			ret = nvmlDeviceGetHandleByIndex(i, &nvml_devices[i]);
			if(ret)log_error("nvmlDeviceGetHandleByIndex(%d) = %d", i, ret);
		}

	}
	if(adl)
	{
		EvilMacro(adl, ADL_MAIN_CONTROL_CREATE,
			ADL_Main_Control_Create, "ADL_Main_Control_Create");
		EvilMacro(adl, ADL_MAIN_CONTROL_DESTROY,
			ADL_Main_Control_Destroy, "ADL_Main_Control_Destroy");
		EvilMacro(adl, ADL_ADAPTER_NUMBEROFADAPTERS_GET,
			ADL_Adapter_NumberOfAdapters_Get,
			"ADL_Adapter_NumberOfAdapters_Get");
		EvilMacro(adl, ADL_ADAPTER_ADAPTERINFO_GET,
			ADL_Adapter_AdapterInfo_Get, "ADL_Adapter_AdapterInfo_Get");
		EvilMacro(adl, ADL_OVERDRIVE_CAPS,
			ADL_Overdrive_Caps, "ADL_Overdrive_Caps");
		EvilMacro(adl, ADL_OVERDRIVE5_TEMPERATURE_GET,
			ADL_Overdrive5_Temperature_Get, "ADL_Overdrive5_Temperature_Get");
		EvilMacro(adl, ADL_OVERDRIVE5_THERMALDEVICES_ENUM,
			ADL_Overdrive5_ThermalDevices_Enum, "ADL_Overdrive5_ThermalDevices_Enum");
		EvilMacro(adl, ADL_OVERDRIVE6_TEMPERATURE_GET,
			ADL_Overdrive6_Temperature_Get, "ADL_Overdrive6_Temperature_Get");

		int ret = ADL_Main_Control_Create(ADL_Main_Memory_Alloc, 1);
		if(ret)log_error("ADL_Main_Control_Create = %d", ret);
		ret = ADL_Adapter_NumberOfAdapters_Get(&adl_device_count);
		if(ret)log_error("ADL_Adapter_NumberOfAdapters_Get = %d", ret);

		adl_odver = malloc(sizeof(int)*adl_device_count);
		adl_domain_id = malloc(sizeof(int)*adl_device_count);
		adl_devid = malloc(sizeof(int)*adl_device_count);
		adl_gputemp = malloc(sizeof(int)*adl_device_count);


		AdapterInfo *adl_inf;
		adl_inf = malloc(sizeof(AdapterInfo)*adl_device_count);
		memset(adl_inf, 0, sizeof(AdapterInfo)*adl_device_count);
		ADL_Adapter_AdapterInfo_Get(adl_inf,
			sizeof(AdapterInfo)*adl_device_count);
		for(int i=0; i < adl_device_count; i++)
		{
			adl_gputemp[i] = 0;
			adl_devid[i] = adl_inf[i].iAdapterIndex;
			adl_odver[i] = 0;
			int odsup=0, oden=0;
			ret = ADL_Overdrive_Caps(adl_devid[i],&odsup,&oden,&adl_odver[i]);
			if(ret)log_error("ADL_Overdrive_Caps(%d) = %d", i, ret);
			if(5 == adl_odver[i])
			{
				int j;
				for(j=0; j<10; j++)
				{
					ADLThermalControllerInfo tci = {0};
					tci.iSize = sizeof(ADLThermalControllerInfo);
					ADL_Overdrive5_ThermalDevices_Enum(adl_devid[i], j, &tci);
					if(tci.iThermalDomain == ADL_DL_THERMAL_DOMAIN_GPU)
					{
						adl_domain_id[i] = j;
						break;
					}
				}
			}
		}
		free(adl_inf);

	}

	gpuinfo_tick();
}

#ifdef _WIN32
#define dlclose FreeLibrary
#endif

void gpuinfo_end(void)
{
	if(nvml)	// for Nvidia GPU temperature
	{
		free(nvml_devices);
		free(nvml_gputemp);
		nvmlShutdown();
		dlclose(nvml);
	}

	if(adl)	// for AMD GPU temperature
	{
		free(adl_odver);
		free(adl_domain_id);
		free(adl_devid);
		free(adl_gputemp);
		int ret = ADL_Main_Control_Destroy();
		if(ret)log_error("ADL_Main_Control_Destroy = %d", ret);

		dlclose(adl);
	}
}



