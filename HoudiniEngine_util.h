#ifndef __HOUDINIENGINE_UTIL__
#define __HOUDINIENGINE_UTIL__

#define ENSURE_SUCCESS(result) \
	if ((result) != HAPI_RESULT_SUCCESS) \
{ \
	std::cout << "failure at " << __FILE__ << ":" << __LINE__ << std::endl; \
	std::cout << hapi::Failure::lastErrorMessage() << std::endl;\
}

#define GET_MAXSCRIPT_NODE(pNode) "mynode68K = maxOps.getNodeByHandle("<<pNode->GetHandle()<<")\n"


namespace util
{
	std::string GetString(int string_handle);
	int FindParm(std::vector<HAPI_ParmInfo>& parms, const char* name, int instanceNum = -1);
	Point3 GetVertexNormal(Mesh* mesh, int faceNo, RVertex* rv);
	void BuildBoxMesh(Mesh& mesh);
	void BuildLogoMesh(Mesh& mesh);
	void BuildMeshFromCookResult( Mesh& mesh, HAPI_AssetId asset_id, float scl, bool forceUpdate = false );
	Mtl* CreateMaterial( HAPI_AssetId asset_id, TSTR& textureWorkPath );
	std::string GanerateClassID(std::string& classname);
	std::string GetProfileString(const MCHAR* key);
	int GetProfileInt(const MCHAR* key);

};

#endif // __HOUDINIENGINE_UTIL__
