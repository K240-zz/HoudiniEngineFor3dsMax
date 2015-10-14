#include "HoudiniEngine.h"
#include "HoudiniEngine_mesh.h"
#include <TlHelp32.h>


HINSTANCE hInstance;
int controlsInit = FALSE;

BOOL WINAPI DllMain(HINSTANCE hinstDLL,ULONG fdwReason,LPVOID lpvReserved)
{
	switch ( fdwReason )
	{
	case DLL_PROCESS_ATTACH:
		{
#if MAX_VERSION_MAJOR >= 17
			MaxSDK::Util::UseLanguagePackLocale();
#endif
			hInstance = hinstDLL;				// Hang on to this DLL's instance handle.
			DisableThreadLibraryCalls(hInstance);


			if ( !controlsInit ) {
				controlsInit = TRUE;
				InitCommonControls();			// Initialize Win95 controls
			}

			hapi::Engine::create();
		}
		break;
	case DLL_PROCESS_DETACH:
		{
			if ( hapi::Engine::instance() )
			{
				hapi::Engine::instance()->release();
			}
			//::TerminateProcess(hinstDLL, 1);

		}
		break;
	}

	return TRUE;
}

__declspec( dllexport ) const TCHAR* LibDescription()
{
	return GetString(IDS_LIBDESCRIPTION);
}

__declspec( dllexport ) int LibNumberClasses()
{
	return 2;
}

__declspec( dllexport ) ClassDesc* LibClassDesc(int i)
{
	switch(i) {
		case 0: return GetHoudiniEngineMeshDesc();
		default: return nullptr;
	}
}

__declspec( dllexport ) ULONG LibVersion()
{
	return VERSION_3DSMAX;
}

__declspec( dllexport ) ULONG CanAutoDefer()
{
   return 0;
}


TCHAR *GetString(int id)
{
	static TCHAR buf[256];

	if (hInstance)
		return LoadString(hInstance, id, buf, _countof(buf)) ? buf : NULL;
	return NULL;
}

