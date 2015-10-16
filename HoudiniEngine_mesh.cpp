#include "HoudiniEngine.h"
#include <icustattribcontainer.h>
#include <custattrib.h>
#include <maxscript/maxscript.h>
#include "HoudiniEngine_mesh.h"

using namespace std;

class HoudiniEngineMeshClassDesc : public ClassDesc2 
{
public:
	virtual int IsPublic() 							{ return FALSE; }
	virtual void* Create(BOOL /*loading = FALSE*/) 	{ return new HoudiniEngineMesh(); }
	virtual const MCHAR *	ClassName() 			{ return GetString(IDS_CLASS_NAME_MESH); }
	virtual SClass_ID SuperClassID() 				{ return GEOMOBJECT_CLASS_ID; }
	virtual Class_ID ClassID() 						{ return HOUDINIENGINE_MESH_CLASS_ID; }
	virtual const MCHAR* Category() 				{ return GetString(IDS_CATEGORY); }
	virtual const MCHAR* InternalName() 			{ return GetString(IDS_CLASS_NAME_MESH); }
	virtual HINSTANCE HInstance() 					{ return hInstance; }
};

ClassDesc2* GetHoudiniEngineMeshDesc() { 
	static HoudiniEngineMeshClassDesc HoudiniEngineMeshDesc;
	return &HoudiniEngineMeshDesc; 
}


enum { houdiniengine_params };
enum {
	ui_asset,
	ui_input,
};
enum {
	pb_filename,
	pb_updatetime,
	pb_conv_unit_i,
	pb_conv_unit_o,
	pb_texture_path,
	pb_auto_update,
	pb_bypass
};

static ParamBlockDesc2 houdiniengine_param_blk ( 
	houdiniengine_params, _T("Houdini Engine"),  0, GetHoudiniEngineMeshDesc(), 
	P_AUTO_CONSTRUCT+P_AUTO_UI+P_MULTIMAP, PBLOCK_REF, 
	//rollout
	1,
	ui_asset, IDD_PANEL_MESH, IDS_HE, 0, 0, NULL,
	//ui_input, IDD_PANEL_MESH_INPUTS, IDS_HE_INPUTS, 0, APPENDROLL_CLOSED, NULL,
	// params
	pb_filename, 		_T("filename"),		TYPE_STRING, 	0,  IDS_FILENAME,
    p_default,			_T(""),
	p_ui,				ui_asset, TYPE_EDITBOX,		IDC_FILE_EDIT,
	p_end, 
	pb_updatetime, 		_T("update_time"),		TYPE_BOOL, 	0,  IDS_HE_UPDATETIME,
    p_default,			true,
	p_ui,				ui_asset, TYPE_SINGLECHEKBOX,		IDC_TIME_UPDATE,
	p_end, 
	pb_conv_unit_i, 	_T("conv_unit_scale_i"),		TYPE_BOOL, 	0,  IDS_HE_CONV_UNIT_I,
    p_default,			true,
	p_ui,				ui_asset, TYPE_SINGLECHEKBOX,		IDC_CONV_UNIT_I,
	p_end, 
	pb_conv_unit_o, 	_T("conv_unit_scale_o"),		TYPE_BOOL, 	0,  IDS_HE_CONV_UNIT_O,
    p_default,			true,
	p_ui,				ui_asset, TYPE_SINGLECHEKBOX,		IDC_CONV_UNIT_O,
	p_end, 
	pb_texture_path,	_T("texture_path"),		TYPE_STRING, 	0,  IDS_TEXTUREPATH,
    p_default,			_T(""),
	p_ui,				ui_asset, TYPE_EDITBOX,		IDC_TEXTUREPATH_EDIT,
	p_end, 
	pb_auto_update,		_T("autoupdate"), TYPE_BOOL, 0, IDS_HE_AUTOUPDATE,
	p_default,			true,
	p_ui,				ui_asset, TYPE_SINGLECHEKBOX, IDC_AUTOUPDATE,
	p_end,
	pb_bypass,			_T("bypass"), TYPE_BOOL, 0, IDS_HE_BYPASS,
	p_default,			false,
	p_ui,				ui_asset, TYPE_SINGLECHEKBOX, IDC_BYPASS,
	p_end,
	p_end
	);

class MainDlgProc : public ParamMap2UserDlgProc {
public:
	HoudiniEngineMesh *obj;		
	MainDlgProc(HoudiniEngineMesh *m) {obj = m;}		
	INT_PTR DlgProc(TimeValue t,IParamMap2 *map,HWND hWnd,UINT msg,WPARAM wParam,LPARAM lParam);		
	void DeleteThis() {
		delete this;
	}
};

//Class for interactive creation of the object using the mouse
class HoudiniEngineMeshCreateCallBack : public CreateMouseCallBack {
	IPoint2 sp0;		//First point in screen coordinates
	HoudiniEngineMesh *ob;		//Pointer to the object 
	Point3 p0;			//First point in world coordinates
public:	
	int proc( ViewExp *vpt,int msg, int point, int flags, IPoint2 m, Matrix3& mat);
	void SetObj(HoudiniEngineMesh *obj) {ob = obj;}
};

int HoudiniEngineMeshCreateCallBack::proc(ViewExp *vpt,int msg, int point, int flags, IPoint2 m, Matrix3& mat )
{
	if (msg==MOUSE_POINT||msg==MOUSE_MOVE) {
		switch(point) {
		case 0: // only happens with MOUSE_POINT msg
			ob->SetSuspendSnap(TRUE);
			sp0 = m;
			p0 = vpt->SnapPoint(m,m,NULL,SNAP_IN_PLANE);
			mat.SetTrans(p0);
			return CREATE_STOP;
			break;
		}
	} else {
		if (msg == MOUSE_ABORT) return CREATE_ABORT;
	}

	return TRUE;
}

static HoudiniEngineMeshCreateCallBack HoudiniEngineMeshCreateCB;

//From BaseObject
CreateMouseCallBack* HoudiniEngineMesh::GetCreateMouseCallBack() 
{
	HoudiniEngineMeshCreateCB.SetObj(this);
	return(&HoudiniEngineMeshCreateCB);
}

IObjParam *HoudiniEngineMesh::ip			= NULL;

int MyEnumProc::proc(ReferenceMaker *rmaker)
{
	if (rmaker->SuperClassID() == BASENODE_CLASS_ID)
	{
		Nodes.Append(1, (INode **)&rmaker);
		if (mbDoHalt)
			return DEP_ENUM_HALT;
	}

	return DEP_ENUM_CONTINUE;
}



HoudiniEngineMesh::HoudiniEngineMesh()
{
	hapi::Engine* engine = hapi::Engine::instance();
	if ( engine )
	{
		if ( !engine->isInitialize() )
		{
			engine->initializeFromIniFile();
			engine->syncTimeline();
		}
	}
	assetId   = -1;
	needUpdateInputNode = false;
	buildingMesh		= false;
	custAttributeUpdate	= false;
	reCook				= false;
	outScale			= 1.0;
	hProgress			= 0;
	//pblock2 = NULL;
	GetHoudiniEngineMeshDesc()->MakeAutoParamBlocks(this);


}

HoudiniEngineMesh::~HoudiniEngineMesh()
{
	inputs.release();
	hapi::Engine* engine = hapi::Engine::instance();
	if ( engine )
	{
		engine->destroyAsset(assetId);
	}
}

void HoudiniEngineMesh::BeginEditParams(IObjParam *ip,ULONG flags,Animatable *prev)
{
	this->ip = ip;
	SimpleObject2::BeginEditParams(ip,flags,prev);

	this->hinstance = GetHoudiniEngineMeshDesc()->HInstance();
	GetHoudiniEngineMeshDesc()->BeginEditParams(ip, this, flags, prev);
	houdiniengine_param_blk.SetUserDlgProc(ui_asset, new MainDlgProc(this));
}

void HoudiniEngineMesh::EndEditParams( IObjParam *ip, ULONG flags,Animatable *next )
{
	SimpleObject2::EndEditParams(ip,flags,next);
	
	GetHoudiniEngineMeshDesc()->EndEditParams(ip, this, flags, next);
	houdiniengine_param_blk.SetUserDlgProc(ui_asset, nullptr);
	this->ip = NULL;
}

DWORD WINAPI fn(LPVOID arg)
{
	return(0);
}

#if defined(USE_NOTIFYREFCHANGED)

int HoudiniEngineMesh::NumRefs()
{
	return 1 + inputs.getNumInputs();
}

RefTargetHandle HoudiniEngineMesh::GetReference(int i)
{ 
	if (i == 0)
		return (RefTargetHandle)pblock;
	else if (i <= inputs.getNumInputs())
		return inputs.getINode(i - 1);
	else
		return nullptr;
}

#endif

void HoudiniEngineMesh::BuildMesh(TimeValue t) 
{
	if ( buildingMesh )
		return;

	bool conv_unit_i = pblock2->GetInt(pb_conv_unit_i) != 0;
	bool conv_unit_o = pblock2->GetInt(pb_conv_unit_o) != 0;
	bool time_update = pblock2->GetInt(pb_updatetime, t) ? true : false;
	bool bypass	     = pblock2->GetInt(pb_bypass, t) ? true : false;

	if (bypass)
		reCook = true;

	hapi::Engine* engine = hapi::Engine::instance();
	buildingMesh = true;

	if (!engine || !engine->isInitialize() || bypass)
	{
		util::BuildLogoMesh(mesh);
		mesh.InvalidateTopologyCache();
		buildingMesh  = false;
		return;
	}

	bool new_loading = LoadAsset();

	if ( assetId >= 0 )
	{
		INode* selfNode = GetINode();
		Matrix3 baseTM(1);

		if (selfNode != nullptr)
			baseTM = Inverse(selfNode->GetObjectTM(t));


		float hapi_time;
		float max_time = TicksToSec(t);
		bool cook = UpdateParameters(t) | new_loading | reCook;

		HAPI_GetTime(hapi::Engine::instance()->session(), &hapi_time);


		if ( time_update && hapi_time != max_time )
		{
			HAPI_SetTime(hapi::Engine::instance()->session(), TicksToSec(t));
			cook = true;
		}
		// Cooking
		if ( cook )
		{
			hapi::Asset asset(assetId);
			if ( asset.isValid() )
			{
				try
				{
					asset.cook();
					int status;
					int wait_count = 0;
					int start_progress = 10;
					bool progressBar = false;
					do {
						Sleep(2);
						HAPI_GetStatus(hapi::Engine::instance()->session(), HAPI_STATUS_COOK_STATE, &status);

						if ( start_progress == wait_count )
						{
							StartProgress();
							progressBar = true;
						}
						if ( progressBar )
						{
							int ccount;
							int tcount;
							HAPI_GetCookingCurrentCount(hapi::Engine::instance()->session(), &ccount);
							HAPI_GetCookingTotalCount(hapi::Engine::instance()->session(), &tcount);
							int progress = (int)(((float)ccount / (float)tcount) * 100.f);
							UpdateProgress( progress );
						}
						wait_count ++;
					} while ( status > HAPI_STATE_MAX_READY_STATE );

					if ( progressBar )
					{
						EndProgress();
					}

					if ( status != HAPI_STATE_READY )
					{
						// Cooking Error
						cook = false;
					}
				}
				catch( hapi::Failure& e)
				{
					// Cooking Error
					e.lastErrorMessage();
					cook = false;
				}
			}
			else
				cook = false;
		}

		// dont need check cook flag, will check hasGeoChanged flag
		// if ( cook )
		{
			int verts = mesh.getNumVerts();
			double scl = conv_unit_o ? GetRelativeScale( UNITS_METERS, 1, GetUSDefaultUnit(), 1 ) : 1.0;
			util::BuildMeshFromCookResult( mesh, assetId, (float)scl, new_loading || (scl != outScale) || reCook );
			outScale = (float)scl;
			if (mesh.getNumVerts() && verts != mesh.getNumVerts())
			{
				mesh.InvalidateTopologyCache();
			}
		}
	}
	if ( !mesh.getNumVerts() )
	{
		util::BuildLogoMesh( mesh);
		mesh.InvalidateTopologyCache();
	}
	ivalid.Set(t,t);
	buildingMesh = false;
	reCook = false;
}

bool HoudiniEngineMesh::LoadAsset()
{
	bool loaded = false;
	hapi::Engine* engine = hapi::Engine::instance();
	if ( engine )
	{
		TSTR wfilename = pblock2->GetStr(pb_filename);
		CStr cfilename = CStr::FromMSTR(wfilename);
		if ( (assetId < 0 || otlFilename != wfilename) && cfilename.length() )
		{
			// delete previous assets
			inputs.release();
			engine->destroyAsset(assetId);
			// loading asset
			int library_id = engine->loadAssetLibrary( cfilename.data() );
			if ( library_id >= 0 )
			{
				otlFilename = wfilename;

				// sync animation setting
				engine->syncTimeline();
				std::string asset_name = engine->getAssetName( library_id, 0 );
				assetId = engine->instantiateAsset( asset_name.c_str(), true );
				if (assetId >= 0)
				{
					inputs.setAssetId( assetId );
					//UpdateInputUI();
					loaded = true;
				}
			}
		}
	}
	return loaded;
}

bool HoudiniEngineMesh::UpdateParameters(TimeValue t)
{
	bool need_cook = false;
	if ( assetId >= 0 )
	{
		hapi::Asset asset(assetId);
		if (asset.isValid())
		{
			INode* node = this->GetINode();
			if (node)
			{
				MSTR pname;
				int subs = node->NumSubs();
				for (int i = 0; i < subs; ++i)
				{
					pname = node->SubAnimName(i);
					Animatable* anim = node->SubAnim(i);
					if (anim)
					{
						int blocks = anim->NumParamBlocks();
						for (int block = 0; block < blocks; ++block)
						{
							IParamBlock2 *pblock = anim->GetParamBlock(block);
							if (pblock) {
								TSTR pbname = pblock->GetLocalName();
								int nNumParams = pblock->NumParams();
								ParamID id;
								std::map<std::string, hapi::Parm> params = asset.parmMap();
								for (int i = 0; i < nNumParams; i++)
								{
									id = pblock->IndextoID(i);
									MSTR pname = pblock->GetLocalName(id);
									std::string hname = CStr::FromMSTR(pblock->GetLocalName(id)).data();

									int index = (int)(hname[hname.size() - 1] - '0');
									hname.pop_back();

									if (std::string("__he_input") == hname)
									{
										// input node
										INode* inputnode = pblock->GetINode(id, t);
										if (inputnode)
										{
											need_cook = SetInputNode(index, inputnode) || need_cook;
										}
									}
									else
									{
										// parameters
										if (params.find(hname) != params.end())
										{
											hapi::Parm parm = params[hname];
											if (HAPI_ParmInfo_IsInt(&parm.info()))
											{
												int value = pblock->GetInt(id, t);
												if (value != parm.getIntValue(index))
												{
													parm.setIntValue(index, value);
													need_cook = true;
												}
											}
											else if (HAPI_ParmInfo_IsFloat(&parm.info()))
											{
												float value = pblock->GetFloat(id, t);
												if (value != parm.getFloatValue(index))
												{
													parm.setFloatValue(index, value);
													need_cook = true;
												}
											}
											else if (HAPI_ParmInfo_IsString(&parm.info()))
											{
												std::string value = CStr::FromMSTR(pblock->GetStr(id, t));
												if (value != parm.getStringValue(index))
												{
													parm.setStringValue(index, value.c_str());
													need_cook = true;
												}
											}
										}
									}
								}
							}
						}
					}
				}
			}
		}
	}
	return need_cook;
}

bool HoudiniEngineMesh::SetInputNode(int ch, INode* node)
{
	bool result = false;
	// Input Nodes
	if (inputs.getAssetId() >= 0)
	{
		TimeValue t = GetCOREInterface()->GetTime();
		INode * selfNode = GetINode();
		Matrix3 baseTM(1);
		
		if (selfNode)
			baseTM = selfNode->GetObjectTM(t);

		bool conv_unit_i = pblock2->GetInt(pb_conv_unit_i) != 0;
		double scl = conv_unit_i ? GetRelativeScale(GetUSDefaultUnit(), 1, UNITS_METERS, 1) : 1.0;
		inputs.setNode(ch, node, t, baseTM, scl, needUpdateInputNode );
		result = true;
#if defined(USE_NOTIFYREFCHANGED)
		if (node->TestForLoop(FOREVER, this) == REF_SUCCEED) {
			ReplaceReference(1+ch, (RefTargetHandle)node);
			NotifyDependents(FOREVER, PART_ALL, REFMSG_CHANGE);
			NotifyDependents(FOREVER, 0, REFMSG_SUBANIM_STRUCTURE_CHANGED);
		}
#endif
	}
	return result;
}


INode* HoudiniEngineMesh::GetINode()
{
	INode* selfNode = nullptr;
	MyEnumProc dep(true);
	
	DoEnumDependents(&dep);
	if (dep.Nodes.Count() > 0)
		selfNode = dep.Nodes[0];

	return selfNode;
}

bool HoudiniEngineMesh::CreateMaterial()
{
	bool result = false;

	if ( assetId >= 0 )
	{
		INode* node = GetINode();
		if ( node )
		{
			TSTR texturePath = pblock2->GetStr(pb_texture_path);
			Mtl* mat = util::CreateMaterial( assetId, texturePath );
			node->SetMtl(mat);
			result = true;
		}
	}
	return result;
}

BOOL HoudiniEngineMesh::OKtoDisplay(TimeValue t) 
{
	return TRUE;
}

void HoudiniEngineMesh::InvalidateUI() 
{
	houdiniengine_param_blk.InvalidateUI();
}

RefTargetHandle HoudiniEngineMesh::Clone(RemapDir& remap) 
{
	HoudiniEngineMesh* newob = new HoudiniEngineMesh();	
	newob->ReplaceReference(0,remap.CloneRef(pblock));
	newob->ivalid.SetEmpty();
	BaseClone(this, newob, remap);
	return(newob);
}

#if defined(USE_NOTIFYREFCHANGED)

#if MAX_VERSION_MAJOR >= 17
RefResult HoudiniEngineMesh::NotifyRefChanged(Interval changeInt, RefTargetHandle hTarget, PartID& partID,  RefMessage message, BOOL propagate)
#else
RefResult HoudiniEngineMesh::NotifyRefChanged(Interval changeInt, RefTargetHandle hTarget, PartID& partID,  RefMessage message)
#endif
{				
	switch (message) {
	case REFMSG_CHANGE:
		{ 
			if (hTarget == pblock2) 
			{
				ParamID last_parm = pblock2->LastNotifyParamID();
				switch (last_parm)
				{
				case pb_filename:
					{
						// do not care this parameter.
					}
					break;
				default:
#if MAX_VERSION_MAJOR == 15
					UpdateAsset(GetCOREInterface()->GetTime());
#endif
					break;
				}
			}
			break;
		}
	default: 
#if MAX_VERSION_MAJOR >= 17
		return SimpleObject2::NotifyRefChanged( changeInt, hTarget, partID, message, propagate);
#else
		return SimpleObject2::NotifyRefChanged( changeInt, hTarget, partID, message);
#endif
	}
	return REF_SUCCEED;
}

#endif

Object* createHoudiniGeom(const char* filename)
{
	HoudiniEngineMesh* geom = new HoudiniEngineMesh();
	return (Object*)geom;
}

INT_PTR MainDlgProc::DlgProc(
	TimeValue t,IParamMap2 *map,HWND hWnd,UINT msg,WPARAM wParam,LPARAM lParam)

{
	switch (msg) {
	case WM_INITDIALOG:
		{
			obj->hParam = hWnd;
			obj->hProgress = GetDlgItem( hWnd, IDC_PROGRESS );
			break;
		}
	case WM_CREATE:
		{
			break;
		}
	case WM_CUSTEDIT_ENTER :
		break;

	case CC_SPINNER_BUTTONDOWN:
		break;

	case CC_SPINNER_CHANGE:
		break;

	case CC_SPINNER_BUTTONUP:
		break;

	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDC_RESET_BUTTON:
			{
				if(HIWORD(wParam) == BN_CLICKED) 
				{
					obj->ResetSimulation(t);
					obj->NotifyDependents (FOREVER, PART_DISPLAY, REFMSG_CHANGE); 
				}
			}
			break;
		case IDC_UPDATE_BUTTON:
			{
				if(HIWORD(wParam) == BN_CLICKED) 
				{
					obj->UpdateAsset(t);
					obj->NotifyDependents (FOREVER, PART_DISPLAY, REFMSG_CHANGE); 
				}
			}
			break;
		case IDC_CREATE_MATERIAL_BUTTON:
			{
				if ( obj->CreateMaterial() )
				{
					obj->NotifyDependents (FOREVER, PART_DISPLAY, REFMSG_CHANGE); 
				}
			}
			break;
		default:
			break;
		}
		break;
	case WM_CLOSE:
		{
			obj->hProgress = 0;
			EndDialog(hWnd, 0);
			break;
		}
	}
	return FALSE;
}

