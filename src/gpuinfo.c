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

#ifdef WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#define HMODULE void*
#endif

HMODULE nvml=NULL;
HMODULE adl=NULL;

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


void gpuinfo_tick(void)
{

	if(nvml)
	{
		for(int i=0; i < nvml_device_count; i++)
		{
			int ret = nvmlDeviceGetTemperature(nvml_devices[i], 0, 
				&nvml_gputemp[i]);
			if(ret)printf("nvmlDeviceGetTemperature(%d) = %d\n", i, ret);
//			printf("Temperature[%d] is %d\n", i, temp);
		}

	}
}


#ifdef WIN32

#define EvilMacro(T, N, NS) \
	N = (T)GetProcAddress(nvml, NS); \
	if(!N)printf("GetProcAddress(\"N\") failed\n")

#else

#define EvilMacro(T, N, NS) \
	*(void **)(&N) = dlsym(nvml, NS); \
	if(!N)printf("dlsym(\"N\") failed\n")

#endif

void gpuinfo_init(void)
{
#ifdef WIN32
	nvml = LoadLibrary("%ProgramFiles%/NVIDIA Corporation/NVSMI/nvml.dll");
	adl = LoadLibrary("atiadlxx.dll");
	if(!adl) adl = LoadLibrary("atiadlxy.dll");	// for 32bit systems
#else
	nvml = dlopen("libnvidia-ml.so", RTLD_LAZY);
	adl = dlopen("libatiadlxx.so", RTLD_LAZY);
#endif

	if(nvml)
	{
		EvilMacro(NVI, nvmlInit, "nvmlInit");
		EvilMacro(NVS, nvmlShutdown, "nvmlShutdown");
		EvilMacro(NVDGC, nvmlDeviceGetCount, "nvmlDeviceGetCount");
		EvilMacro(NVDGHBI, nvmlDeviceGetHandleByIndex,
			"nvmlDeviceGetHandleByIndex");
		EvilMacro(NVDGT, nvmlDeviceGetTemperature,
			"nvmlDeviceGetTemperature");

		int ret = nvmlInit();
		if(ret)printf("nvmlInit = %d\n", ret);

		ret = nvmlDeviceGetCount(&nvml_device_count);
		if(ret)printf("nvmlDeviceGetCount = %d\n", ret);
		
		nvml_devices = malloc(sizeof(nvmlDevice_t)*nvml_device_count);
		nvml_gputemp = malloc(sizeof(int)*nvml_device_count);
		for(int i=0; i < nvml_device_count; i++)
		{
			ret = nvmlDeviceGetHandleByIndex(i, &nvml_devices[i]);
			if(ret)printf("nvmlDeviceGetHandleByIndex(%d) = %d\n", i, ret);
		}

	}
	else printf("no nvml\n");
	if(adl)
	{


	}
	else printf("no adl\n");

	gpuinfo_tick();
}

#ifdef WIN32
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
		dlclose(adl);
	}
}



