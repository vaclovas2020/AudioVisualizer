// header.h : include file for standard system include files,
// or project specific include files
//

#pragma once

#include "targetver.h"
#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
// Windows Header Files
#include <windows.h>
// C RunTime Header Files
#include <stdlib.h>
#include <malloc.h>
#include <memory.h>
#include <tchar.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <avrt.h>
#include <vector>
#include <thread>
#include <atomic>
#pragma comment(lib, "avrt.lib")