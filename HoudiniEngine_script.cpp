#include "HoudiniEngine.h"
#include "HoudiniEngine_util.h"
#include "HoudiniEngine_gui.h"

#define HOUDINIENGINE_FP_INTERFACE_ID Interface_ID(0x661f5198, 0x78814977)

class HoudiniEngineFunctionInterface : public FPStaticInterface {
	public:
		DECLARE_DESCRIPTOR(HoudiniEngineFunctionInterface);

		enum OpID {
			kInitialize,
			kCleanup,
			kIsInitialize,
			kGetLastResult,
			kGetLastError,
			kLoadAssetLibrary,
			kGetAvailableAssetCount,
			kGetAssetName,
			kInstantiateAsset,
			kDestroyAsset,
			kGeneratePluginScript,
			};

		static BOOL Initialize();
		static int Cleanup();
		static BOOL IsInitialize();
		static int GetLastResult();
		static TSTR GetLastError();
		static int LoadAssetLibrary( const MCHAR* otl_path );
		static int GetAvailableAssetCount( int library_id );
		static TSTR GetAssetName( int library_id, int index );
		static int InstantiateAsset(  const MCHAR* name, BOOL cook_on_load );
		static BOOL DestroyAsset( int asset_id );
		static BOOL GeneratePluginScript(const MCHAR* otl_filename, const MCHAR* name, const MCHAR* category, const MCHAR* texture_path, const MCHAR* mxs_filename);

		BEGIN_FUNCTION_MAP			

			FN_0(kInitialize, TYPE_BOOL, Initialize)
			FN_0(kCleanup, TYPE_INT, Cleanup)
			FN_0(kIsInitialize, TYPE_BOOL, IsInitialize)
			FN_0(kGetLastResult, TYPE_INT, GetLastResult)
			FN_0(kGetLastError, TYPE_STRING, GetLastError)
			FN_1(kLoadAssetLibrary, TYPE_INT, LoadAssetLibrary, TYPE_STRING)
			FN_1(kGetAvailableAssetCount, TYPE_INT, GetAvailableAssetCount, TYPE_INT)
			FN_2(kGetAssetName, TYPE_STRING, GetAssetName, TYPE_INT, TYPE_INT)
			FN_2(kInstantiateAsset, TYPE_INT, InstantiateAsset, TYPE_STRING, TYPE_BOOL)
			FN_1(kDestroyAsset, TYPE_BOOL, DestroyAsset, TYPE_INT)
			FN_5(kGeneratePluginScript, TYPE_BOOL, GeneratePluginScript, TYPE_STRING, TYPE_STRING, TYPE_STRING, TYPE_STRING, TYPE_STRING)

		END_FUNCTION_MAP

};

static HoudiniEngineFunctionInterface theHoudiniEngineFunctionInterface (
	HOUDINIENGINE_FP_INTERFACE_ID, _T("HoudiniEngine"), 0, 0, FP_CORE,
	HoudiniEngineFunctionInterface::kInitialize, _T("Initialize"), 0, TYPE_BOOL, 0, 0,
	HoudiniEngineFunctionInterface::kCleanup, _T("Cleanup"), 0, TYPE_VOID, 0, 0,
	HoudiniEngineFunctionInterface::kIsInitialize, _T("IsInitialize"), 0, TYPE_BOOL, 0, 0,
	HoudiniEngineFunctionInterface::kGetLastResult, _T("GetLastResult"), 0, TYPE_INT, 0, 0,
	HoudiniEngineFunctionInterface::kGetLastError, _T("GetLastError"), 0, TYPE_STRING, 0, 0,
	HoudiniEngineFunctionInterface::kLoadAssetLibrary, _T("LoadAssetLibrary"), 0, TYPE_INT, 0, 1,
		_T("otl_path"), 0, TYPE_STRING,
	HoudiniEngineFunctionInterface::kGetAvailableAssetCount, _T("GetAvailableAssetCount"), 0, TYPE_INT, 0, 1,
		_T("library_id"), 0, TYPE_INT,
	HoudiniEngineFunctionInterface::kGetAssetName, _T("GetAssetName"), 0, TYPE_STRING, 0, 2,
		_T("library_id"), 0, TYPE_INT,
		_T("index"), 0, TYPE_INT,
	HoudiniEngineFunctionInterface::kInstantiateAsset, _T("InstantiateAsset"), 0, TYPE_INT, 0, 2,
		_T("name"), 0, TYPE_STRING,
		_T("cook_on_load"), 0, TYPE_BOOL,
	HoudiniEngineFunctionInterface::kDestroyAsset, _T("DestroyAsset"), 0, TYPE_BOOL, 0, 1,
		_T("asset_id"), 0, TYPE_INT,

	HoudiniEngineFunctionInterface::kGeneratePluginScript, _T("GeneratePluginScript"), 0, TYPE_BOOL, 0, 5,
		_T("otl_filename"), 0, TYPE_STRING,
		_T("name"), 0, TYPE_STRING,
		_T("category"), 0, TYPE_STRING,
		_T("texture_path"), 0, TYPE_STRING,
		_T("mxs_filename"), 0, TYPE_STRING,
	p_end);

#include <fstream>

BOOL HoudiniEngineFunctionInterface::GeneratePluginScript(const MCHAR* otl_filename, const MCHAR* name, const MCHAR* category, const MCHAR* texture_path, const MCHAR* mxs_filename)
{
	BOOL result = false;
	hapi::Engine* engine = hapi::Engine::instance();
	if (engine)
	{
		std::string cotl_filename = CStr::FromMCHAR(otl_filename).data();
		std::string cname = CStr::FromMCHAR(name).data();
		std::string ccategory = CStr::FromMCHAR(category).data();
		std::string ctexture_path = CStr::FromMCHAR(texture_path).data();
		std::string cmxs_filename = CStr::FromMCHAR(mxs_filename).data();
		std::string code;
		std::string classid;

		classid = util::GanerateClassID(cname);

		if (classid.size() > 0)
			result = GenerateScriptPlugin(cotl_filename, cname, ccategory, ctexture_path, classid, code);

		if (result)
		{
			std::ofstream ofs(cmxs_filename);

			if (ofs)
			{
				ofs << code;
			}
			else
				result = false;
		}
	}
	return result;
}

BOOL HoudiniEngineFunctionInterface::Initialize()
{
	BOOL result = FALSE;
	hapi::Engine* engine = hapi::Engine::instance();
	if ( engine )
	{
		result = engine->initializeFromIniFile() ? TRUE : FALSE;
	}
	return result;
}

int HoudiniEngineFunctionInterface::Cleanup()
{
	BOOL result = FALSE;
	hapi::Engine* engine = hapi::Engine::instance();
	if ( engine )
	{
		engine->cleanup();
		result = TRUE;
	}
	return result;
}

BOOL HoudiniEngineFunctionInterface::IsInitialize()
{
	BOOL result = FALSE;
	hapi::Engine* engine = hapi::Engine::instance();
	if ( engine )
	{
		result = engine->isInitialize() ? TRUE : FALSE;
	}
	return result;
}

int HoudiniEngineFunctionInterface::GetLastResult()
{
	int result = HAPI_RESULT_FAILURE;
	hapi::Engine* engine = hapi::Engine::instance();
	if ( engine )
	{
		result = engine->getLastResult();
	}
	return result;
}

TSTR HoudiniEngineFunctionInterface::GetLastError()
{
	TSTR result;
	hapi::Engine* engine = hapi::Engine::instance();
	if ( engine )
	{
		result = TSTR::FromACP( engine->getLastError().c_str() );
	}
	return result;
}

int HoudiniEngineFunctionInterface::LoadAssetLibrary( const MCHAR* otl_path )
{
	int library_id = -1;
	hapi::Engine* engine = hapi::Engine::instance();
	if ( engine )
	{
		CStr otl_cpath = CStr::FromMCHAR( otl_path );
		library_id = engine->loadAssetLibrary( otl_cpath.data() );
	}
	return library_id;
}

int HoudiniEngineFunctionInterface::GetAvailableAssetCount( int library_id )
{
	int result = 0;
	hapi::Engine* engine = hapi::Engine::instance();
	if ( engine )
	{
		result = engine->getAvailableAssetCount( library_id );
	}
	return result;
}

TSTR HoudiniEngineFunctionInterface::GetAssetName( int library_id, int index )
{
	// TYPE_STRING needs static.... does not thread safe so far.
	static TSTR result;
	hapi::Engine* engine = hapi::Engine::instance();
	if ( engine )
	{
		result = TSTR::FromACP( engine->getAssetName( library_id, index ).c_str() );
	}

	return result;
}

int HoudiniEngineFunctionInterface::InstantiateAsset(  const MCHAR* name, BOOL cook_on_load )
{
	int result = HAPI_RESULT_FAILURE;
	hapi::Engine* engine = hapi::Engine::instance();
	if ( engine )
	{
		CStr cname = CStr::FromMCHAR( name );
		result = engine->instantiateAsset( cname, cook_on_load == TRUE );

	}
	return result;
}

BOOL HoudiniEngineFunctionInterface::DestroyAsset( int asset_id )
{
	BOOL result = FALSE;
	hapi::Engine* engine = hapi::Engine::instance();
	if ( engine )
	{
		result = engine->destroyAsset( asset_id ) ? TRUE : FALSE;
	}
	return result;
}




