#include <sstream>
#include "HoudiniEngine.h"
#include "HoudiniEngine_input.h"
#include "HoudiniEngine_util.h"

class NullView: public View {
public:
	Point2 ViewToScreen(Point3 p) { return Point2(p.x,p.y); }
	NullView() { worldToView.IdentityMatrix(); screenW=640.0f; screenH = 480.0f; }
};

HAPI_AssetId InputMesh( int asset_id, int input_id, INode* node, TimeValue t, double scale, InputAsset* iasset = NULL )
{
	HAPI_AssetId asset;

	if ( iasset && iasset->node && iasset->asset_id >= 0 )
	{
		// Update
		asset = iasset->asset_id;
	}
	else
	{
		HAPI_CreateInputAsset(hapi::Engine::instance()->session(), &asset, NULL);
	}

	Object *pobj = node->EvalWorldState(t).obj;
	BOOL needDel;
	NullView nullView;
	Mesh *msh = ((GeomObject*)pobj)->GetRenderMesh(t,node,nullView,needDel);
	msh->buildNormals();

    // set up part info
    HAPI_PartInfo partInfo;
    HAPI_PartInfo_Init(&partInfo);
    partInfo.id = 0;
    partInfo.faceCount        = msh->numFaces;
    partInfo.vertexCount      = msh->numFaces*3;
    partInfo.pointCount       = msh->numVerts;

    // copy data to arrays
	{
		std::vector<int> vl;
		std::vector<int> fc;
		std::vector<float> pt;

		vl.reserve( partInfo.vertexCount );
		fc.reserve( partInfo.faceCount );
		pt.reserve( partInfo.pointCount*3 );

		// convert unit scaling
		float scl = (float)scale;
		// build vertex and face count
		for ( int i = 0; i < msh->numFaces; ++i )
		{
			fc.push_back( 3 );
			for ( int j = 2; j >= 0; --j )
			{
	    		vl.push_back( msh->faces[i].v[j] );
			}
		}
		for ( int i = 0; i < msh->numVerts; ++i )
		{
			Point3 p = msh->verts[i];
			float y = p.y;
			p.y = p.z;
			p.z = -y;
			p *= scl;

			pt.push_back( p.x );
			pt.push_back( p.y );
			pt.push_back( p.z );
		}
		// Set the data
		HAPI_SetPartInfo(hapi::Engine::instance()->session(), asset, 0, 0, &partInfo);
		HAPI_SetFaceCounts(hapi::Engine::instance()->session(), asset, 0, 0, &fc.front(), 0, partInfo.faceCount);
		HAPI_SetVertexList(hapi::Engine::instance()->session(), asset, 0, 0, &vl.front(), 0, partInfo.vertexCount);
		// Set position attributes.
		HAPI_AttributeInfo pos_attr_info;
		pos_attr_info.exists             = true;
		pos_attr_info.owner              = HAPI_ATTROWNER_POINT;
		pos_attr_info.storage            = HAPI_STORAGETYPE_FLOAT;
		pos_attr_info.count              = partInfo.pointCount;
		pos_attr_info.tupleSize          = 3;
		HAPI_AddAttribute(hapi::Engine::instance()->session(), asset, 0, 0, "P", &pos_attr_info);
		HAPI_SetAttributeFloatData(hapi::Engine::instance()->session(), asset, 0, 0, "P", &pos_attr_info, &pt.front(), 0, partInfo.pointCount);
	}
	// normals
    {

        // build the per-vertex normals
        std::vector<float> vertexNormals;
        vertexNormals.reserve(partInfo.vertexCount * 3);

		for ( int i = 0; i < msh->numFaces; ++i )
		{
			for ( int j = 2; j >= 0; --j )
			{
	    		int vi = msh->faces[i].v[j];
				Point3 vn = util::GetVertexNormal(msh, i, msh->getRVertPtr(vi));
				vertexNormals.push_back(vn.x);
				vertexNormals.push_back(vn.z);
				vertexNormals.push_back(-vn.y);
			}
        }

        // add and set it to HAPI
        HAPI_AttributeInfo attributeInfo;
        attributeInfo.exists = true;
        attributeInfo.owner = HAPI_ATTROWNER_VERTEX;
        attributeInfo.storage = HAPI_STORAGETYPE_FLOAT;
        attributeInfo.count = partInfo.vertexCount;
        attributeInfo.tupleSize = 3;
		HAPI_AddAttribute(hapi::Engine::instance()->session(), asset, 0, 0, "N", &attributeInfo);
		HAPI_SetAttributeFloatData(hapi::Engine::instance()->session(), asset, 0, 0, "N", &attributeInfo,
                &vertexNormals.front(), 0, partInfo.vertexCount);
    }
	// uv
	{
		// uv range 1 to (MAX_MESHMAPS-1)
		int useMaps = 0;
		for (int i = 1; useMaps < msh->getNumMaps() || i < MAX_MESHMAPS; ++i)
		{
			if (msh->mapSupport(i))
			{
				if (msh->getNumMapVerts(i))
				{
					std::string uvName;
					std::string uvNumberName;
					uvName = i == 1 ? std::string("uv") : std::string("uv") + std::to_string(i);
					uvNumberName = i == 1 ? std::string("uvNumber") : std::string("uvNumber") + std::to_string(i);

					std::vector<int> uvn;
					std::vector<float> uvv;

					for (int f = 0; f < msh->numFaces; ++f)
					{
						for (int deg = 3-1; deg >= 0; --deg)
						{
							int vv = msh->mapFaces(i)[f].getTVert(deg);
							uvv.push_back(msh->mapVerts(i)[vv].x);
							uvv.push_back(msh->mapVerts(i)[vv].y);
							uvv.push_back(0.0f);
							uvn.push_back(vv);
						}
					}

					if (partInfo.vertexCount == uvn.size())
					{
						// add and set it to HAPI
						HAPI_AttributeInfo attributeInfo;
						attributeInfo.exists = true;
						attributeInfo.owner = HAPI_ATTROWNER_VERTEX;
						attributeInfo.storage = HAPI_STORAGETYPE_FLOAT;
						attributeInfo.count = partInfo.vertexCount;
						attributeInfo.tupleSize = 3;
						HAPI_AddAttribute(hapi::Engine::instance()->session(), asset, 0, 0, uvName.c_str(), &attributeInfo);
						HAPI_SetAttributeFloatData(hapi::Engine::instance()->session(), asset, 0, 0, uvName.c_str(), &attributeInfo,
							&uvv.front(), 0, partInfo.vertexCount);

						attributeInfo.storage = HAPI_STORAGETYPE_INT;
						attributeInfo.count = partInfo.vertexCount;
						attributeInfo.tupleSize = 1;
						HAPI_AddAttribute(hapi::Engine::instance()->session(), asset, 0, 0, uvNumberName.c_str(), &attributeInfo);
						HAPI_SetAttributeIntData(hapi::Engine::instance()->session(), asset, 0, 0, uvNumberName.c_str(), &attributeInfo,
							&uvn.front(), 0, partInfo.vertexCount);
					}
				}
				useMaps++;
			}
		}
	}
	// smooting group and material id
	{
		std::vector<int>	sg;
		std::vector<int>	mid;
		sg.reserve( partInfo.faceCount );
		mid.reserve( partInfo.faceCount );

		for ( int i = 0; i < msh->numFaces; ++i )
		{
			for ( int j = 0; j < 3; ++j )
			{
	    		sg.push_back( (int)msh->faces[i].getSmGroup() );
	    		mid.push_back( (int)msh->faces[i].getMatID() );
			}
        }
        HAPI_AttributeInfo attributeInfo;
        attributeInfo.exists = true;
        attributeInfo.owner = HAPI_ATTROWNER_PRIM;
        attributeInfo.storage = HAPI_STORAGETYPE_INT;
        attributeInfo.count = msh->numFaces;
        attributeInfo.tupleSize = 1;
		HAPI_AddAttribute(hapi::Engine::instance()->session(), asset, 0, 0, "max_sg", &attributeInfo);
		HAPI_SetAttributeIntData(hapi::Engine::instance()->session(), asset, 0, 0, "max_sg", &attributeInfo,
                &sg.front(), 0, msh->numFaces);
        attributeInfo.exists = true;
        attributeInfo.owner = HAPI_ATTROWNER_PRIM;
        attributeInfo.storage = HAPI_STORAGETYPE_INT;
        attributeInfo.count = msh->numFaces;
        attributeInfo.tupleSize = 1;
		HAPI_AddAttribute(hapi::Engine::instance()->session(), asset, 0, 0, "max_mid", &attributeInfo);
		HAPI_SetAttributeIntData(hapi::Engine::instance()->session(), asset, 0, 0, "max_mid", &attributeInfo,
                &mid.front(), 0, msh->numFaces);
	}

	HAPI_CommitGeo(hapi::Engine::instance()->session(), asset, 0, 0);
	HAPI_ConnectAssetGeometry(hapi::Engine::instance()->session(), asset, 0, asset_id, input_id);

	if (needDel) delete msh;

	return asset;
}

HAPI_AssetId InputPoly( int asset_id, int input_id, INode* node, TimeValue t, double scale, InputAsset* iasset = NULL )
{
	HAPI_AssetId asset;

	if ( iasset && iasset->node && iasset->asset_id >= 0 )
	{
		// Update
		asset = iasset->asset_id;
	}
	else
	{
		HAPI_CreateInputAsset(hapi::Engine::instance()->session(), &asset, NULL);
	}

	Object *pobj = node->EvalWorldState(t).obj;
	PolyObject *poly = (PolyObject*)pobj;
	MNMesh &msh = poly->GetMesh();


    // set up part info
    HAPI_PartInfo partInfo;
    HAPI_PartInfo_Init(&partInfo);
    partInfo.id = 0;
    partInfo.faceCount        = msh.numf;
    partInfo.vertexCount      = 0;
    partInfo.pointCount       = msh.numv;

	// compute vertices
	for ( int i = 0; i < msh.numf; i++ )
	{
		partInfo.vertexCount += msh.f[i].deg;
	}
    // copy data to arrays
	{
		std::vector<int> vl;
		std::vector<int> fc;
		std::vector<float> pt;

		vl.reserve( partInfo.vertexCount );
		fc.reserve( partInfo.faceCount );
		pt.reserve( partInfo.pointCount*3 );

		// convert unit scaling
		float scl = (float)scale;
		// build vertex and face count
		for ( int i = 0; i < msh.numf; i++ )
		{
			fc.push_back( msh.f[i].deg );
			for ( int j = msh.f[i].deg-1; j >= 0; --j )
			{
				vl.push_back( msh.f[i].vtx[j] );
			}
		}
		for ( int i = 0; i < msh.numv; i++ )
		{
			Point3 p = msh.v[i].p;
			float y = p.y;
			p.y = p.z;
			p.z = -y;
			p *= scl;

			pt.push_back( p.x );
			pt.push_back( p.y );
			pt.push_back( p.z );
		}
		// Set the data
		HAPI_SetPartInfo(hapi::Engine::instance()->session(), asset, 0, 0, &partInfo);
		HAPI_SetFaceCounts(hapi::Engine::instance()->session(), asset, 0, 0, &fc.front(), 0, partInfo.faceCount);
		HAPI_SetVertexList(hapi::Engine::instance()->session(), asset, 0, 0, &vl.front(), 0, partInfo.vertexCount);
		// Set position attributes.
		HAPI_AttributeInfo pos_attr_info;
		pos_attr_info.exists    = true;
		pos_attr_info.owner     = HAPI_ATTROWNER_POINT;
		pos_attr_info.storage   = HAPI_STORAGETYPE_FLOAT;
		pos_attr_info.count     = partInfo.pointCount;
		pos_attr_info.tupleSize = 3;
		HAPI_AddAttribute(hapi::Engine::instance()->session(), asset, 0, 0, "P", &pos_attr_info);
		HAPI_SetAttributeFloatData(hapi::Engine::instance()->session(), asset, 0, 0, "P", &pos_attr_info, &pt.front(), 0, partInfo.pointCount);
	}
	// normals
    {
		MNNormalSpec* nrmspec = 0;
		nrmspec = msh.GetSpecifiedNormals();
		if (!nrmspec)
		{
			msh.SpecifyNormals();
			nrmspec = msh.GetSpecifiedNormals();
			if (nrmspec)
				nrmspec->CheckNormals();

		}
		if (nrmspec && nrmspec->GetNumFaces() == msh.numf)
		{
			// build the per-vertex normals
			std::vector<float> vertexNormals;
			vertexNormals.reserve(partInfo.vertexCount * 3);

			for ( int f = 0; f < msh.numf; f++ )
			{
				MNNormalFace& nf = nrmspec->Face(f);
				int deg = nf.GetDegree();

				for(int i = deg-1; i >= 0; i--)
				{
					Point3 n;

					MNNormalFace& nf = nrmspec->Face(f);
					n = nrmspec->Normal(nf.GetNormalID(i));
					vertexNormals.push_back(n.x);
					vertexNormals.push_back(n.z);
					vertexNormals.push_back(-n.y);
				}
			}
			// add and set it to HAPI
			HAPI_AttributeInfo attributeInfo;
			attributeInfo.exists    = true;
			attributeInfo.owner     = HAPI_ATTROWNER_VERTEX;
			attributeInfo.storage   = HAPI_STORAGETYPE_FLOAT;
			attributeInfo.count     = partInfo.vertexCount;
			attributeInfo.tupleSize = 3;
			HAPI_AddAttribute(hapi::Engine::instance()->session(), asset, 0, 0, "N", &attributeInfo);
			HAPI_SetAttributeFloatData(hapi::Engine::instance()->session(), asset, 0, 0, "N", &attributeInfo,
					&vertexNormals.front(), 0, partInfo.vertexCount);
		}
    }
	// uv
	{
		// uv range 1 to (MAX_MESHMAPS-1)
		for (int i = 1; i < MAX_MESHMAPS; ++i)
		{
			MNMap* uv = msh.M(i);
			if (uv && uv->numv && msh.numf == uv->numf)
			{
				std::string uvName;
				std::string uvNumberName;
				uvName = i == 1 ? std::string("uv") : std::string("uv") + std::to_string(i);
				uvNumberName = i == 1 ? std::string("uvNumber") : std::string("uvNumber") + std::to_string(i);

				std::vector<int> uvn;
				std::vector<float> uvv;

				for (int f = 0; f < uv->numf; ++f)
				{
					for (int deg = msh.f[i].deg - 1; deg >= 0; --deg)
					{
						int vv = uv->f[f].tv[deg];
						uvv.push_back(uv->v[vv].x);
						uvv.push_back(uv->v[vv].y);
						uvv.push_back(0.0f);
						uvn.push_back(vv);
					}
				}

				if (partInfo.vertexCount == uvn.size())
				{
					// add and set it to HAPI
					HAPI_AttributeInfo attributeInfo;
					attributeInfo.exists = true;
					attributeInfo.owner = HAPI_ATTROWNER_VERTEX;
					attributeInfo.storage = HAPI_STORAGETYPE_FLOAT;
					attributeInfo.count = partInfo.vertexCount;
					attributeInfo.tupleSize = 3;
					HAPI_AddAttribute(hapi::Engine::instance()->session(), asset, 0, 0, uvName.c_str(), &attributeInfo);
					HAPI_SetAttributeFloatData(hapi::Engine::instance()->session(), asset, 0, 0, uvName.c_str(), &attributeInfo,
						&uvv.front(), 0, partInfo.vertexCount);

					attributeInfo.storage = HAPI_STORAGETYPE_INT;
					attributeInfo.count = partInfo.vertexCount;
					attributeInfo.tupleSize = 1;
					HAPI_AddAttribute(hapi::Engine::instance()->session(), asset, 0, 0, uvNumberName.c_str(), &attributeInfo);
					HAPI_SetAttributeIntData(hapi::Engine::instance()->session(), asset, 0, 0, uvNumberName.c_str(), &attributeInfo,
						&uvn.front(), 0, partInfo.vertexCount);
				}
			}
		}
	}

	// smooting group and material id
	{
		std::vector<int>	sg;
		std::vector<int>	mid;
		sg.reserve( partInfo.faceCount );
		mid.reserve( partInfo.faceCount );

		for ( int i = 0; i < msh.numf; ++i )
		{
			sg.push_back( (int)msh.f[i].smGroup );
			mid.push_back( (int)msh.f[i].material );
        }
        HAPI_AttributeInfo attributeInfo;
        attributeInfo.exists    = true;
        attributeInfo.owner     = HAPI_ATTROWNER_PRIM;
        attributeInfo.storage   = HAPI_STORAGETYPE_INT;
        attributeInfo.count     = msh.numf;
        attributeInfo.tupleSize = 1;
		HAPI_AddAttribute(hapi::Engine::instance()->session(), asset, 0, 0, "max_sg", &attributeInfo);
		HAPI_SetAttributeIntData(hapi::Engine::instance()->session(), asset, 0, 0, "max_sg", &attributeInfo,
                &sg.front(), 0, msh.numf);
        attributeInfo.exists    = true;
        attributeInfo.owner     = HAPI_ATTROWNER_PRIM;
        attributeInfo.storage   = HAPI_STORAGETYPE_INT;
        attributeInfo.count     = msh.numf;
        attributeInfo.tupleSize = 1;
		HAPI_AddAttribute(hapi::Engine::instance()->session(), asset, 0, 0, "max_mid", &attributeInfo);
		HAPI_SetAttributeIntData(hapi::Engine::instance()->session(), asset, 0, 0, "max_mid", &attributeInfo,
                &mid.front(), 0, msh.numf);
	}

	HAPI_CommitGeo(hapi::Engine::instance()->session(), asset, 0, 0);
	HAPI_ConnectAssetGeometry(hapi::Engine::instance()->session(), asset, 0, asset_id, input_id);

	return asset;
}

HAPI_AssetId InputCurve( int asset_id, int input_id, INode* node, TimeValue t, double scale,  InputAsset* iasset = NULL )
{
	HAPI_AssetId asset = -1;

	Object *pobj = node->EvalWorldState(t).obj;
	ShapeObject *so = (ShapeObject *)pobj;
	{
		BezierShape shape;

		if(so->CanMakeBezier()) 
			so->MakeBezier(t, shape);
		else
		{
			PolyShape pshape;
			so->MakePolyShape(t, pshape);
			shape = pshape;
		}

		HAPI_AssetInfo myCurveAssetInfo;
		HAPI_NodeInfo myCurveNodeInfo;

		if ( iasset && iasset->node && iasset->asset_id >= 0 )
		{
			// Update
			asset = iasset->asset_id;
		}
		else
		{
			// New
			HAPI_CreateCurve(hapi::Engine::instance()->session(), &asset);
		}
		HAPI_GetAssetInfo(hapi::Engine::instance()->session(), asset, &myCurveAssetInfo);
		HAPI_GetNodeInfo(hapi::Engine::instance()->session(), myCurveAssetInfo.nodeId, &myCurveNodeInfo);
		HAPI_ConnectAssetGeometry(hapi::Engine::instance()->session(), asset, 0, asset_id, input_id);


		// find coords parm
		std::vector<HAPI_ParmInfo> parms(myCurveNodeInfo.parmCount);
		HAPI_GetParameters(hapi::Engine::instance()->session(), myCurveNodeInfo.id, &parms[0], 0, myCurveNodeInfo.parmCount);
		int typeParmIndex = util::FindParm(parms, "type");
		int coordsParmIndex = util::FindParm(parms, "coords");
		int orderParmIndex = util::FindParm(parms, "order");
		int closeParmIndex = util::FindParm(parms, "close");
		if(coordsParmIndex < 0
			|| coordsParmIndex < 0
			|| orderParmIndex < 0
			|| closeParmIndex < 0)
		{
			return -1;
		}

		const HAPI_ParmInfo &typeParm = parms[typeParmIndex];
		const HAPI_ParmInfo &coordsParm = parms[coordsParmIndex];
		const HAPI_ParmInfo &orderParm = parms[orderParmIndex];
		const HAPI_ParmInfo &closeParm = parms[closeParmIndex];

		// type
		{
			HAPI_ParmChoiceInfo* choices = new HAPI_ParmChoiceInfo[typeParm.choiceCount];
			HAPI_GetParmChoiceLists(hapi::Engine::instance()->session(), myCurveNodeInfo.id, choices, typeParm.choiceIndex, typeParm.choiceCount);

			int typeIdx = -1;
			for(int i = 0; i < typeParm.choiceCount; i++)
			{
				if( util::GetString(choices[i].valueSH) == "bezier")
				{
					typeIdx = i;
					break;
				}
			}

			delete [] choices;

			if(typeIdx < 0)
			{
				return -1;
			}

			HAPI_SetParmIntValues(hapi::Engine::instance()->session(), myCurveNodeInfo.id, &typeIdx, typeParm.intValuesIndex, 1);
			{
				// convert unit scaling
				float scl = (float)scale;
				int polys = shape.SplineCount();
				for(int poly = 0; poly < polys; ++poly) 
				{
					Spline3D *spline = shape.GetSpline(poly);
					int knots = spline->KnotCount();
					std::ostringstream coords;
					for(int ix = 0; ix < knots; ++ix) 
					{
						Point3 p;
						if ( ix > 0 )
						{
							p = spline->GetInVec(ix);
							float y = p.y;
							p.y = p.z;
							p.z = -y;
							p *= scl;
							coords << p.x << "," << p.y << "," << p.z << " ";
						}
						p = spline->GetKnotPoint(ix);
						float y = p.y;
						p.y = p.z;
						p.z = -y;
						p *= scl;
						coords << p.x << "," << p.y << "," << p.z << " ";
						if ( ix < (knots-1) )
						{
							p = spline->GetOutVec(ix);
							float y = p.y;
							p.y = p.z;
							p.z = -y;
							p *= scl;
							coords << p.x << "," << p.y << "," << p.z << " ";
						}
					}
					HAPI_SetParmStringValue(hapi::Engine::instance()->session(), myCurveNodeInfo.id, coords.str().c_str(), coordsParm.id, coordsParm.stringValuesIndex);
					break;
				}
			}
			// order
			{
				int order = 4;
				HAPI_SetParmIntValues(hapi::Engine::instance()->session(), myCurveNodeInfo.id, &order, orderParm.intValuesIndex, 1);
			}

			// periodicity
			{
				int close = 0;
				HAPI_SetParmIntValues(hapi::Engine::instance()->session(), myCurveNodeInfo.id, &close, closeParm.intValuesIndex, 1);
			}
		}
	}
	return asset;
}




HAPI_AssetId InputNode( int asset_id, int input_id, INode* node, TimeValue t, double scale, InputAsset* iasset)
{
	HAPI_AssetId asset = -1;
	HAPI_AssetInfo assetInfo;
	HAPI_GetAssetInfo(hapi::Engine::instance()->session(), asset_id, &assetInfo);

	if (node)
    {
		Object *pobj = node->EvalWorldState(t).obj;
		if ( pobj->SuperClassID() == GEOMOBJECT_CLASS_ID )
		{
			if (pobj->IsSubClassOf(polyObjectClassID)) 
			{
				asset = InputPoly( assetInfo.id, input_id, node, t, scale, iasset );
			}
			else
			{
				asset = InputMesh( assetInfo.id, input_id, node, t, scale, iasset );
			}
		}
		else if ( pobj->SuperClassID() == SHAPE_CLASS_ID )                    
		{
			asset = InputCurve( assetInfo.id, input_id, node, t, scale, iasset );
		}
	}
	return asset;
}

InputAssets::InputAssets() : assetId(-1)
{
}

InputAssets::~InputAssets()
{
	release();
}

void InputAssets::release()
{
	for ( int i = 0; i < inputs.size(); ++i )
	{
		disconnect( i, true );
	}
	inputs.clear();
	assetId = -1;
}

void InputAssets::setAssetId( int asset_id )
{
	if ( asset_id < 0 )
		return;

	if ( assetId != asset_id )
	{
		release();
	}
	else
		return;

	assetId = asset_id;
	HAPI_GetAssetInfo(hapi::Engine::instance()->session(), assetId, &assetInfo);
	if ( assetInfo.geoInputCount > 0 )
	{
		inputs.resize( assetInfo.geoInputCount );
		for ( int i = 0; i < inputs.size(); ++i )
		{
			inputs[i].asset_id = -1;
			inputs[i].node = NULL;
		}
	}
}

bool InputAssets::setNode( int ch, INode* node, TimeValue t, double scale, bool check_v_update )
{
	bool result = false;
	if ( ch < inputs.size() )
	{
		if ( inputs[ch].node != node )
		{
			if ( inputs[ch].node )
				disconnect( ch, true );

			if ( node )
			{
				HAPI_AssetId id = InputNode( assetInfo.id, ch, node, t, scale );
				if ( id >= 0 )
				{
					inputs[ch].asset_id = id;
					inputs[ch].node = node;
				}
			}
			result = true;
		}
		else if ( check_v_update && inputs[ch].node == node )
		{
			InputNode( assetInfo.id, ch, node, t, scale, &inputs[ch] );
			result = true;
		}
	}
	return result;
}



void InputAssets::disconnect( int ch, bool free_node )
{
	if ( assetId >= 0 && inputs[ch].asset_id >= 0 )
	{
		HAPI_DisconnectAssetGeometry(hapi::Engine::instance()->session(), assetId, ch);

		if ( free_node && inputs[ch].asset_id >= 0 )
		{
			HAPI_DestroyAsset(hapi::Engine::instance()->session(), inputs[ch].asset_id);
			inputs[ch].asset_id = -1;
		}
	}
}
