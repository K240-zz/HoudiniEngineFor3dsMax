
#ifndef __HoudiniEngineMesh__H
#define __HoudiniEngineMesh__H
#include "istdplug.h"
#include "iparamb2.h"
#include "iparamm2.h"
#include "Simpmod.h"
#include "Simpobj.h"
#include "meshadj.h"
#include "XTCObject.h"

#include "HoudiniEngine.h"
#include "HoudiniEngine_gui.h"
#include "HoudiniEngine_input.h"
#include "HoudiniEngine_util.h"

#define HOUDINENGINE_INPUT_MAX		(10)
#define THREAD_ASSET				(1)

#define PBLOCK_REF  SIMPMOD_PBLOCKREF

class HoudiniEngineMesh : public SimpleObject2
{
public:
	static IObjParam					*ip;

	//Constructor/Destructor
	HoudiniEngineMesh();
	virtual ~HoudiniEngineMesh();	

	virtual void DeleteThis() { delete this; }	

	// From BaseObject
	virtual CreateMouseCallBack* GetCreateMouseCallBack();
	virtual const MCHAR *GetObjectName() { return GetString(IDS_CLASS_NAME_GEOM); }
	//Interval GetValidity(TimeValue t);

	// From Object
	virtual Interval ObjectValidity(TimeValue t) { Interval iv; iv.Set(t,t); return iv; }

	// From Animatable
	virtual void BeginEditParams( IObjParam  *ip, ULONG flags,Animatable *prev);
	virtual void EndEditParams( IObjParam *ip, ULONG flags,Animatable *next);

	//From Animatable
	virtual Class_ID ClassID() {return HOUDINIENGINE_MESH_CLASS_ID;}		
	virtual SClass_ID SuperClassID() { return GEOMOBJECT_CLASS_ID; }
	virtual void GetClassName(TSTR& s) {s = GetString(IDS_CLASS_NAME_GEOM);}

	virtual RefTargetHandle Clone( RemapDir &remap );
#if defined(USE_NOTIFYREFCHANGED)

#if MAX_VERSION_MAJOR >= 17
	virtual RefResult NotifyRefChanged(Interval changeInt, RefTargetHandle hTarget, PartID& partID,  RefMessage message, BOOL propagate);
#else
	virtual RefResult NotifyRefChanged(Interval changeInt, RefTargetHandle hTarget, PartID& partID,  RefMessage message);
#endif
#endif
	virtual int	NumParamBlocks() { return 1; }					// return number of ParamBlocks in this instance
	virtual IParamBlock2* GetParamBlock(int i) { return pblock2; } // return i'th ParamBlock
	virtual IParamBlock2* GetParamBlockByID(BlockID id) { return (pblock2->ID() == id) ? pblock2 : NULL; } // return id'd ParamBlock	

	void BuildMesh(TimeValue t);
	BOOL OKtoDisplay(TimeValue t);
	void InvalidateUI();

	// local methods
	BOOL GetSuspendSnap(){return suspendSnap;}
	void SetSuspendSnap(BOOL iSuspendSnap){suspendSnap = iSuspendSnap;}

	int	asset_id() { return assetId; }
	void ResetSimulation(TimeValue t)
	{
		if ( assetId >= 0 )
		{
			HAPI_ResetSimulation(hapi::Engine::instance()->session(), assetId);
			if ( !buildingMesh )
				BuildMesh(t);
		}
	}
	void UpdateAsset(TimeValue t)
	{
		needUpdateInputNode = true;
		if ( !buildingMesh )
			BuildMesh(t);
	}

	void UpdateCustomAttributes(TimeValue t)
	{
		if ( !buildingMesh )
		{
			BuildMesh(t);
			NotifyDependents (FOREVER, PART_DISPLAY, REFMSG_CHANGE); 
		}
	}
	bool LoadAsset();
	bool UpdateParameters(TimeValue t);
	bool CreateMaterial();
	bool SetInputNode(int ch, INode* node);
	INode* GetINode();

private:
	void StartProgress()
	{
		if ( hProgress )
			SendMessage( hProgress, PBM_SETRANGE, 0, MAKELPARAM(0, 100)); 
	}
	void UpdateProgress( int pos )
	{
		if ( hProgress )
			SendMessage( hProgress, PBM_SETPOS, pos, 0 );
	}
	void EndProgress()
	{
		if ( hProgress )
			SendMessage( hProgress, PBM_SETPOS, 100, 0 );
	}

public:
	HWND								hParam;
	HWND								hProgress;
	HINSTANCE							hinstance;
private:
	// Parameter block
	//Class vars
	HAPI_AssetId						assetId;
	InputAssets							inputs;
	bool								needUpdateInputNode;
	bool								buildingMesh;
	bool								custAttributeUpdate;
	TSTR								otlFilename;
	bool								reCook;
	float								outScale;
};


extern ClassDesc2* GetHoudiniEngineMeshDesc();

#endif
