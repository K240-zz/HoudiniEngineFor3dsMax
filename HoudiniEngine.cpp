#include "HoudiniEngine.h"
#include <string>
#include <vector>
#include <map>
#include <stdexcept>
#include "HoudiniEngine_util.h"

namespace hapi {

std::string getString( int string_handle )
{
    if (string_handle == 0)
        return "";

    int buffer_length;
	HAPI_GetStringBufLength(hapi::Engine::instance()->session(), string_handle, &buffer_length);

    char * buf = new char[ buffer_length ];

	HAPI_GetString(hapi::Engine::instance()->session(), string_handle, buf, buffer_length);

    std::string result( buf );
    delete[] buf;
    return result;
}

static void throwOnFailure(HAPI_Result result)
{
    if (result != HAPI_RESULT_SUCCESS)
        throw Failure(result);
}

Asset::Asset(int id)
    : id(id), _info(NULL), _nodeInfo(NULL)
{
}

Asset::Asset(const Asset &asset)
    : id(asset.id)
    , _info(asset._info ? new HAPI_AssetInfo(*asset._info) : NULL)
    , _nodeInfo(asset._nodeInfo ? new HAPI_NodeInfo(*asset._nodeInfo) : NULL)
{
}

Asset::~Asset()
{
    delete this->_info;
    delete this->_nodeInfo;
}

Asset & Asset::operator=(const Asset &asset)
{
    if (this != &asset)
    {
        delete this->_info;
        delete this->_nodeInfo;
        this->id = asset.id;
        this->_info = asset._info ? new HAPI_AssetInfo(*asset._info) : NULL;
        this->_nodeInfo = asset._nodeInfo
                ? new HAPI_NodeInfo(*asset._nodeInfo) : NULL;
    }
    return *this;
}

const HAPI_AssetInfo & Asset::info() const
{
    if (!this->_info)
    {
        this->_info = new HAPI_AssetInfo();
		throwOnFailure(HAPI_GetAssetInfo(hapi::Engine::instance()->session(), this->id, this->_info));
    }
    return *this->_info;
}

const HAPI_NodeInfo & Asset::nodeInfo() const
{
    if (!this->_nodeInfo)
    {
        this->_nodeInfo = new HAPI_NodeInfo();
		throwOnFailure(HAPI_GetNodeInfo(hapi::Engine::instance()->session(),
                           this->info().nodeId, this->_nodeInfo));
    }
    return *this->_nodeInfo;
}

bool Asset::isValid() const
{
    int is_valid = 0;
    try
    {
        // Note that calling info() might fail if the info isn't cached
        // and the asest id is invalid.
		throwOnFailure(HAPI_IsAssetValid(hapi::Engine::instance()->session(),
                           this->id, this->info().validationId, &is_valid));
        return is_valid != 0;
    }
    catch (Failure &failure)
    {
        return false;
    }
}

std::string Asset::name() const
{ return getString(info().nameSH); }

std::string Asset::label() const
{ return getString(info().labelSH); }

std::string Asset::filePath() const
{ return getString(info().filePathSH); }

void Asset::destroyAsset() const
{
	throwOnFailure(HAPI_DestroyAsset(hapi::Engine::instance()->session(), this->id));
}

void Asset::cook() const
{
	throwOnFailure(HAPI_CookAsset(hapi::Engine::instance()->session(), this->id, NULL));
}

HAPI_TransformEuler Asset::getTransform(
        HAPI_RSTOrder rst_order, HAPI_XYZOrder rot_order) const
{
    HAPI_TransformEuler result;
	throwOnFailure(HAPI_GetAssetTransform(hapi::Engine::instance()->session(),
                       this->id, rst_order, rot_order, &result));
    return result;
}

void Asset::getTransformAsMatrix(float result_matrix[16]) const
{
    HAPI_TransformEuler transform =
            this->getTransform( HAPI_SRT, HAPI_XYZ );
	throwOnFailure(HAPI_ConvertTransformEulerToMatrix(hapi::Engine::instance()->session(),
                       &transform, result_matrix ) );
}


std::string Asset::getInputName( int input, int input_type ) const
{
    HAPI_StringHandle name;
	throwOnFailure(HAPI_GetInputName(hapi::Engine::instance()->session(), this->id,
                                      input, input_type,
                                      &name ) );
    return getString(name);
}


std::vector<Object> Asset::objects() const
{
    std::vector<Object> result;
    for (int object_id=0; object_id < info().objectCount; ++object_id)
        result.push_back(Object(*this, object_id));
    return result;
}


std::vector<Parm> Asset::parms() const
{
    // Get all the parm infos.
    int num_parms = nodeInfo().parmCount;
    std::vector<HAPI_ParmInfo> parm_infos(num_parms);
	throwOnFailure(HAPI_GetParameters(hapi::Engine::instance()->session(),
                       this->info().nodeId, &parm_infos[0], /*start=*/0, num_parms));

    // Get all the parm choice infos.
    std::vector<HAPI_ParmChoiceInfo> parm_choice_infos(
                this->nodeInfo().parmChoiceCount);

	if (parm_choice_infos.size() > 0)
	{
		throwOnFailure(HAPI_GetParmChoiceLists(hapi::Engine::instance()->session(),
			this->info().nodeId, &parm_choice_infos[0], /*start=*/0,
			this->nodeInfo().parmChoiceCount));
	}

    // Build and return a vector of Parm objects.
    std::vector<Parm> result;
    for (int i=0; i < num_parms; ++i)
        result.push_back(Parm(
                             this->info().nodeId, parm_infos[i], parm_choice_infos.size() == 0 ? nullptr : &parm_choice_infos[0]));
    return result;
}

std::map<std::string, Parm> Asset::parmMap() const
{
    std::vector<Parm> parms = this->parms();

    std::map<std::string, Parm> result;
    for (int i=0; i < int(parms.size()); ++i)
        result.insert(std::make_pair(parms[i].name(), parms[i]));

    return result;
}

Object::Object(int asset_id, int object_id)
    : asset(asset_id), id(object_id), _info(NULL)
{}

Object::Object(const Asset &asset, int id)
    : asset(asset), id(id), _info(NULL)
{}

Object::Object(const Object &object)
    : asset(object.asset)
    , id(object.id)
    , _info(object._info ? new HAPI_ObjectInfo(*object._info) : NULL)
{}

Object::~Object()
{ delete this->_info; }

Object & Object::operator=(const Object &object)
{
    if (this != &object)
    {
        delete this->_info;
        asset = object.asset;
        this->id = object.id;
        this->_info = object._info
                ? new HAPI_ObjectInfo(*object._info) : NULL;
    }
    return *this;
}

const HAPI_ObjectInfo & Object::info() const
{
    if (!this->_info)
    {
        this->_info = new HAPI_ObjectInfo();
		throwOnFailure(HAPI_GetObjects(hapi::Engine::instance()->session(),
                           this->asset.id, this->_info, this->id, /*length=*/1));
    }
    return *this->_info;
}

std::vector<Geo> Object::geos() const
{
    std::vector<Geo> result;
    for (int geo_id=0; geo_id < info().geoCount; ++geo_id)
        result.push_back(Geo(*this, geo_id));
    return result;
}

std::string Object::name() const
{ return getString(info().nameSH); }

std::string Object::objectInstancePath() const
{ return getString(info().objectInstancePathSH); }


Geo::Geo(const Object &object, int id)
    : object(object), id(id), _info(NULL)
{}

Geo::Geo(int asset_id, int object_id, int geo_id)
    : object(asset_id, object_id), id(geo_id), _info(NULL)
{}

Geo::Geo(const Geo &geo)
    : object(geo.object)
    , id(geo.id)
    , _info(geo._info ? new HAPI_GeoInfo(*geo._info) : NULL)
{}

Geo::~Geo()
{ delete _info; }

Geo & Geo::operator=(const Geo &geo)
{
    if (this != &geo)
    {
        delete this->_info;
        this->object = geo.object;
        this->id = geo.id;
        this->_info = geo._info ? new HAPI_GeoInfo(*geo._info) : NULL;
    }
    return *this;
}

const HAPI_GeoInfo & Geo::info() const
{
    if (!this->_info)
    {
        this->_info = new HAPI_GeoInfo();
		throwOnFailure(HAPI_GetGeoInfo(hapi::Engine::instance()->session(),
                           this->object.asset.id, this->object.id, this->id, this->_info));
    }
    return *this->_info;
}

std::string Geo::name() const
{ return getString(info().nameSH); }


std::vector<Part> Geo::parts() const
{
    std::vector<Part> result;
    for (int part_id=0; part_id < info().partCount; ++part_id)
        result.push_back(Part(*this, part_id));
    return result;
}


Part::Part(const Geo &geo, int id)
    : geo(geo), id(id), _info(NULL)
{}

Part::Part(int asset_id, int object_id, int geo_id, int part_id)
    : geo(asset_id, object_id, geo_id), id(part_id), _info(NULL)
{}

Part::Part(const Part &part)
    : geo(part.geo)
    , id(part.id)
    , _info(part._info ? new HAPI_PartInfo(*part._info) : NULL)
{}

Part::~Part()
{ delete _info; }

Part & Part::operator=(const Part &part)
{
    if (this != &part)
    {
        delete this->_info;
        this->geo = part.geo;
        this->id = part.id;
        this->_info = part._info ? new HAPI_PartInfo(*part._info) : NULL;
    }
    return *this;
}

const HAPI_PartInfo & Part::info() const
{
    if (!this->_info)
    {
        this->_info = new HAPI_PartInfo();
		throwOnFailure(HAPI_GetPartInfo(hapi::Engine::instance()->session(),
                           this->geo.object.asset.id, this->geo.object.id, this->geo.id,
                           this->id, this->_info));
    }
    return *this->_info;
}

std::string Part::name() const
{ return getString(info().nameSH); }

int Part::numAttribs(HAPI_AttributeOwner attrib_owner) const
{
    switch (attrib_owner)
    {
    case HAPI_ATTROWNER_VERTEX:
        return this->info().vertexAttributeCount;
    case HAPI_ATTROWNER_POINT:
        return this->info().pointAttributeCount;
    case HAPI_ATTROWNER_PRIM:
        return this->info().faceAttributeCount;
    case HAPI_ATTROWNER_DETAIL:
        return this->info().detailAttributeCount;
    case HAPI_ATTROWNER_MAX:
    case HAPI_ATTROWNER_INVALID:
        break;
    }

    return 0;
}

std::vector<std::string> Part::attribNames(HAPI_AttributeOwner attrib_owner) const
{
    int num_attribs = numAttribs(attrib_owner);
    std::vector<int> attrib_names_sh(num_attribs);

	throwOnFailure(HAPI_GetAttributeNames(hapi::Engine::instance()->session(),
                       this->geo.object.asset.id, this->geo.object.id, this->geo.id,
                       this->id, attrib_owner, &attrib_names_sh[0], num_attribs));

    std::vector<std::string> result;
    for (int attrib_index=0; attrib_index < int(attrib_names_sh.size());
         ++attrib_index)
        result.push_back(getString(attrib_names_sh[attrib_index]));
    return result;
}

HAPI_AttributeInfo Part::attribInfo(
        HAPI_AttributeOwner attrib_owner, const char *attrib_name) const
{
    HAPI_AttributeInfo result;
	throwOnFailure(HAPI_GetAttributeInfo(hapi::Engine::instance()->session(),
                       this->geo.object.asset.id, this->geo.object.id, this->geo.id,
                       this->id, attrib_name, attrib_owner, &result));
    return result;
}

float * Part::getNewFloatAttribData(
        HAPI_AttributeInfo &attrib_info, const char *attrib_name,
        int start, int length) const
{
    if (length < 0)
        length = attrib_info.count - start;

    float *result = new float[attrib_info.count * attrib_info.tupleSize];
	throwOnFailure(HAPI_GetAttributeFloatData(hapi::Engine::instance()->session(),
                       this->geo.object.asset.id, this->geo.object.id, this->geo.id,
                       this->id, attrib_name, &attrib_info, result,
                       /*start=*/0, attrib_info.count));
    return result;
}

Parm::Parm()
{ }

Parm::Parm(int node_id, const HAPI_ParmInfo &info,
           HAPI_ParmChoiceInfo *all_choice_infos)
    : node_id(node_id), _info(info)
{
    for (int i=0; i < info.choiceCount; ++i)
        this->choices.push_back(ParmChoice(
                                    all_choice_infos[info.choiceIndex + i]));
}

const HAPI_ParmInfo & Parm::info() const
{ return _info; }

std::string Parm::name() const
{ return getString(_info.nameSH); }

std::string Parm::label() const
{ return getString(_info.labelSH); }

int Parm::getIntValue(int sub_index) const
{
    int result;
	throwOnFailure(HAPI_GetParmIntValues(hapi::Engine::instance()->session(),
                       this->node_id, &result, this->_info.intValuesIndex + sub_index,
                       /*length=*/1));
    return result;
}

float Parm::getFloatValue(int sub_index) const
{
    float result;
	throwOnFailure(HAPI_GetParmFloatValues(hapi::Engine::instance()->session(),
                       this->node_id, &result, this->_info.floatValuesIndex + sub_index,
                       /*length=*/1));
    return result;
}

std::string Parm::getStringValue(int sub_index) const
{
    int string_handle;
	throwOnFailure(HAPI_GetParmStringValues(hapi::Engine::instance()->session(),
                       this->node_id, true, &string_handle,
                       this->_info.stringValuesIndex + sub_index, /*length=*/1));
    return getString(string_handle);
}

void Parm::setIntValue(int sub_index, int value)
{
	throwOnFailure(HAPI_SetParmIntValues(hapi::Engine::instance()->session(),
                       this->node_id, &value, this->_info.intValuesIndex + sub_index,
                       /*length=*/1));
}

void Parm::setFloatValue(int sub_index, float value)
{
	throwOnFailure(HAPI_SetParmFloatValues(hapi::Engine::instance()->session(),
                       this->node_id, &value, this->_info.floatValuesIndex + sub_index,
                       /*length=*/1));
}

void Parm::setStringValue(int sub_index, const char *value)
{
	throwOnFailure(HAPI_SetParmStringValue(hapi::Engine::instance()->session(),
                       this->node_id, value, this->_info.id, sub_index));
}

void Parm::insertMultiparmInstance(int instance_position)
{
	throwOnFailure(HAPI_InsertMultiparmInstance(hapi::Engine::instance()->session(),
                       this->node_id, this->_info.id, instance_position));
}

void Parm::removeMultiparmInstance(int instance_position)
{
	throwOnFailure(HAPI_RemoveMultiparmInstance(hapi::Engine::instance()->session(),
                       this->node_id, this->_info.id, instance_position));
}

ParmChoice::ParmChoice(HAPI_ParmChoiceInfo &info)
    : _info(info)
{}

const HAPI_ParmChoiceInfo & ParmChoice::info() const
{ return _info; }

std::string ParmChoice::label() const
{ return getString(_info.labelSH); }

std::string ParmChoice::value() const
{ return getString(_info.valueSH); }


Engine::Engine() : mInitialized(false)
{

}

Engine::~Engine()
{
    cleanup();
}


bool Engine::initializeFromIniFile()
{
	int session_mode = 1;
	std::string otl_search_path;
	std::string dso_search_path;
	std::string image_dso_search_path;
	std::string audio_dso_search_path;
	bool use_cooking_thread;
	int	 cooking_thread_stack_size = -1;
	std::string thrift_address;
	int  thrift_port = 12345;
	std::string thrift_pipe;

	otl_search_path = util::GetProfileStringW(_T("otl_search_path"));
	dso_search_path = util::GetProfileStringW(_T("dso_search_path"));
	image_dso_search_path = util::GetProfileStringW(_T("image_dso_search_path"));
	audio_dso_search_path = util::GetProfileStringW(_T("audio_dso_search_path"));
	use_cooking_thread = util::GetProfileStringW(_T("use_cooking_thread")) == std::string("false") ? false : true;
	session_mode = util::GetProfileIntW(_T("proc_mode"));
	thrift_address = util::GetProfileStringW(_T("thriftsocket_address"));
	thrift_port = util::GetProfileIntW(_T("thriftsocket_port"));
	thrift_pipe = util::GetProfileStringW(_T("thriftpipe_name"));

	return initialize(
		otl_search_path.c_str(),
		dso_search_path.c_str(),
		image_dso_search_path.c_str(),
		audio_dso_search_path.c_str(),
		use_cooking_thread,
		cooking_thread_stack_size,
		session_mode,
		thrift_address.c_str(),
		thrift_port,
		thrift_pipe.c_str()
		);
}

bool Engine::initialize( const char * otl_search_path,
                         const char * dso_search_path,
						 const char * image_dso_search_path,
						 const char * audio_dso_search_path,
                         bool use_cooking_thread,
                         int cooking_thread_stack_size,
						 int session_mode,
						 const char * thrift_address,
						 int thrift_port,
	                     const char * thrift_pipe
	)
{
    if ( !isInitialize() )
    {
		mResult = HAPI_RESULT_FAILURE;
		
		switch (session_mode)
		{
		default:	// In Process
		{
			mResult = HAPI_CreateInProcessSession(&mSession);
			break;
		}
		case 2:		// Thrift Socket
		{
			mResult = HAPI_CreateThriftSocketSession(
				&mSession,
				thrift_address,
				thrift_port
				);
			break;
		}
		case 3:		// Thrift Pipe
		{
			mResult = HAPI_CreateThriftNamedPipeSession(
				&mSession,
				thrift_pipe
				);
			break;
		}
		}
		
		if (mResult == HAPI_RESULT_SUCCESS)
		{
			HAPI_CookOptions cook_options = HAPI_CookOptions_Create();
			mResult = HAPI_Initialize(
				&mSession,
				&cook_options,
				use_cooking_thread,
				cooking_thread_stack_size,
				otl_search_path,
				dso_search_path,
				image_dso_search_path,
				audio_dso_search_path
				);

			mInitialized = true;
		}
    }

    return mInitialized;
}

void Engine::cleanup()
{
    if ( isInitialize() )
    {
		mResult = HAPI_Cleanup(hapi::Engine::instance()->session());
		mResult = HAPI_CloseSession(hapi::Engine::instance()->session());
        mInitialized = false;
    }
}

bool    Engine::isInitialize()
{
	return mInitialized;
#if 0
    mResult = HAPI_IsInitialized();
    return mResult == HAPI_RESULT_SUCCESS;
#endif
}

HAPI_Result     Engine::getLastResult()
{
    return mResult;
}

std::string     Engine::getLastError()
{
    int buffer_length;
	HAPI_GetStatusStringBufLength(hapi::Engine::instance()->session(),
                HAPI_STATUS_CALL_RESULT, HAPI_STATUSVERBOSITY_ERRORS, &buffer_length);

    char * buf = new char[ buffer_length ];

	HAPI_GetStatusString(hapi::Engine::instance()->session(), HAPI_STATUS_CALL_RESULT, buf, buffer_length);

    std::string result( buf );
    delete[] buf;

    return result;
}

int Engine::loadAssetLibrary( const char* otl_file )
{
    std::string otl( otl_file );
    int library_id = -1;

    if ( mAssetLib.find( otl ) != mAssetLib.end() )
    {
        library_id = mAssetLib[ otl ];
    }
    else
    {
		mResult = HAPI_LoadAssetLibraryFromFile(hapi::Engine::instance()->session(),
                    otl_file,
					false,
                    &library_id );

        if ( mResult == HAPI_RESULT_SUCCESS )
        {
            mAssetLib[ otl ] = library_id;
        }
    }

    return library_id;
}

int Engine::getAvailableAssetCount( int library_id )
{
    int count = 0;

	mResult = HAPI_GetAvailableAssetCount(hapi::Engine::instance()->session(), library_id, &count);

    return count;
}

std::string Engine::getAssetName( int library_id, int id )
{
    std::string asset_name;
    int count = getAvailableAssetCount( library_id );

    if ( id >= 0 && id < count )
    {
        HAPI_StringHandle* asset_name_sh = new HAPI_StringHandle[count];

		mResult = HAPI_GetAvailableAssets(hapi::Engine::instance()->session(), library_id, asset_name_sh, count);

        if ( mResult == HAPI_RESULT_SUCCESS )
            asset_name = getString( asset_name_sh[id] );

        delete [] asset_name_sh;
    }

    return asset_name;
}

int Engine::instantiateAsset(  const char* name, bool cook_on_load )
{
    int asset_id = -1;

	mResult = HAPI_InstantiateAsset(hapi::Engine::instance()->session(), name, cook_on_load, &asset_id);

    return asset_id;
}

bool Engine::destroyAsset( int asset_id )
{
	mResult = HAPI_RESULT_SUCCESS;

	if ( asset_id >= 0 )
	{
		mResult = HAPI_DestroyAsset(hapi::Engine::instance()->session(), asset_id);
	}

	return mResult == HAPI_RESULT_SUCCESS;
}

void Engine::syncTimeline()
{
	// Check animatoin range and fps
	HAPI_TimelineOptions timeline;
	HAPI_GetTimelineOptions(hapi::Engine::instance()->session(), &timeline);

	int	fps	= GetFrameRate();
	Interval animRange = GetCOREInterface()->GetAnimRange();
	float frame_start  = TicksToSec(animRange.Start());
	float frame_end    = TicksToSec(animRange.End());

	if ( timeline.fps != (float)fps ||
		timeline.startTime != frame_start ||
		timeline.endTime != frame_end )
	{
		timeline.fps = (float) fps;
		timeline.startTime = frame_start;
		timeline.endTime = frame_end;

		HAPI_SetTimelineOptions(hapi::Engine::instance()->session(), &timeline);
	}
}



static Engine* sInstance = nullptr;

Engine* Engine::instance()
{
	return sInstance;
}

Engine* Engine::create()
{
	if ( !sInstance )
	{
		sInstance = new Engine();
	}
	return sInstance;
}

void Engine::release()
{
	if ( sInstance )
	{
		sInstance->cleanup();
		delete sInstance;
		sInstance = nullptr;
	}
}

HAPI_Session* Engine::session()
{
	return &mSession;
}


};
