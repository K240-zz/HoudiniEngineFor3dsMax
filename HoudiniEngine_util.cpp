#include <iostream>
#include "HoudiniEngine.h"
#include "HoudiniEngine_util.h"
#include <stdmat.h>
#include <maxscript\maxscript.h>
#include <sstream>
#include <IPathConfigMgr.h>

namespace util
{

	std::string GetString(int string_handle)
	{
		// A string handle of 0 means an invalid string handle -- similar to
		// a null pointer.  Since we can't return NULL, though, return an empty
		// string.
		if (string_handle == 0)
			return "";

		int buffer_length;
		ENSURE_SUCCESS(HAPI_GetStringBufLength(hapi::Engine::instance()->session(), string_handle, &buffer_length));

		int string_length = buffer_length - 1;
		std::string result(string_length, '\0');

		ENSURE_SUCCESS(HAPI_GetString(hapi::Engine::instance()->session(), string_handle, &result[0], buffer_length));
		return result;
	}

	int FindParm(std::vector<HAPI_ParmInfo>& parms, const char* name, int instanceNum)
	{
		for(size_t i = 0; i < parms.size(); i++)
		{
			if(GetString(parms[i].templateNameSH) == name
					&& (instanceNum < 0 || parms[i].instanceNum == instanceNum)
			  )
			{
				return static_cast<int>(i);
			}
		}
		return -1;
	}

	// from asciiexp/export.cpp
	Point3 GetVertexNormal(Mesh* mesh, int faceNo, RVertex* rv)
	{
		Face* f = &mesh->faces[faceNo];
		DWORD smGroup = f->smGroup;
		int numNormals = 0;
		Point3 vertexNormal;

		// Is normal specified
		// SPCIFIED is not currently used, but may be used in future versions.
		if (rv->rFlags & SPECIFIED_NORMAL) {
			vertexNormal = rv->rn.getNormal();
		}
		// If normal is not specified it's only available if the face belongs
		// to a smoothing group
		else if ((numNormals = rv->rFlags & NORCT_MASK) != 0 && smGroup) {
			// If there is only one vertex is found in the rn member.
			if (numNormals == 1) {
				vertexNormal = rv->rn.getNormal();
			}
			else {
				// If two or more vertices are there you need to step through them
				// and find the vertex with the same smoothing group as the current face.
				// You will find multiple normals in the ern member.
				for (int i = 0; i < numNormals; i++) {
					if (rv->ern[i].getSmGroup() & smGroup) {
						vertexNormal = rv->ern[i].getNormal();
					}
				}
			}
		}
		else {
			// Get the normal from the Face if no smoothing groups are there
			vertexNormal = mesh->getFaceNormal(faceNo);
		}

		return vertexNormal;
	}

	static BOOL SetNumTVFaces( Mesh& mesh, int oldsize, int newsize )
	{
		if ( !mesh.tvFace )
		{
			return mesh.setNumTVFaces( newsize );
		}

		TVFace* tvface = new TVFace[oldsize];
		for ( int i = 0; i < oldsize; i ++ )
		{
			tvface[i] = mesh.tvFace[i];
		}
		if ( mesh.setNumTVFaces( newsize /*, TRUE, oldsize*/ ) )
		{
			for ( int i = 0; i < oldsize && i < newsize; i ++ )
			{
				mesh.tvFace[i] = tvface[i];
			}
		}
		delete [] tvface;

		return TRUE;
	}

	static void MakeQuad(int nverts, Face *f, int a, int b , int c , int d, int sg, int bias) {
		int sm = 1<<sg;
		assert(a<nverts);
		assert(b<nverts);
		assert(c<nverts);
		assert(d<nverts);
		if (bias) {
			f[0].setVerts( b, a, c);
			f[0].setSmGroup(sm);
			f[0].setEdgeVisFlags(1,0,1);
			f[1].setVerts( d, c, a);
			f[1].setSmGroup(sm);
			f[1].setEdgeVisFlags(1,0,1);
		} else {
			f[0].setVerts( a, b, c);
			f[0].setSmGroup(sm);
			f[0].setEdgeVisFlags(1,1,0);
			f[1].setVerts( c, d, a);
			f[1].setSmGroup(sm);
			f[1].setEdgeVisFlags(1,1,0);
		}
	}


#define POSX 0	// right
#define POSY 1	// back
#define POSZ 2	// top
#define NEGX 3	// left
#define NEGY 4	// front
#define NEGZ 5	// bottom

	int direction(Point3 *v) {
		Point3 a = v[0]-v[2];
		Point3 b = v[1]-v[0];
		Point3 n = CrossProd(a,b);
		switch(MaxComponent(n)) {
		case 0: return (n.x<0)?NEGX:POSX;
		case 1: return (n.y<0)?NEGY:POSY;
		case 2: return (n.z<0)?NEGZ:POSZ;
		}
		return 0;
	}

	// Remap the sub-object material numbers so that the top face is the first one
	// The order now is:
	// Top / Bottom /  Left/ Right / Front / Back
	static int mapDir[6] ={ 3, 5, 0, 2, 4, 1 };

#define MAKE_QUAD(na,nb,nc,nd,sm,b) {MakeQuad(nverts,&(mesh.faces[nf]),na, nb, nc, nd, sm, b);nf+=2;}

	void BuildBoxMesh(Mesh& mesh)
	{
		int ix,iy,iz,nf,kv,mv,nlayer,topStart,midStart;
		int nverts,wsegs,lsegs,hsegs,nv,nextk,nextm,wsp1;
		int nfaces;
		BOOL genUvs;
		Point3 va,vb,p;
		float l, w, h;
		int genUVs = 1;
		BOOL bias = 0;

		float scl = (float)GetRelativeScale( UNITS_METERS, 1, GetUSDefaultUnit(), 1 );
		l = scl;
		h = scl;
		w = scl;
		lsegs = 4;
		wsegs = 4;
		hsegs = 4;
		genUvs = FALSE;
		if (h<0.0f) bias = 1;

		// Number of verts
		// bottom : (lsegs+1)*(wsegs+1)
		// top    : (lsegs+1)*(wsegs+1)
		// sides  : (2*lsegs+2*wsegs)*(hsegs-1)

		// Number of rectangular faces.
		// bottom : (lsegs)*(wsegs)
		// top    : (lsegs)*(wsegs)
		// sides  : 2*(hsegs*lsegs)+2*(wsegs*lsegs)

		wsp1 = wsegs + 1;
		nlayer  =  2*(lsegs+wsegs);
		topStart = (lsegs+1)*(wsegs+1);
		midStart = 2*topStart;

		nverts = midStart+nlayer*(hsegs-1);
		nfaces = 4*(lsegs*wsegs + hsegs*lsegs + wsegs*hsegs);

		mesh.Init();

		mesh.setNumVerts(nverts);
		mesh.setNumFaces(nfaces);
		mesh.InvalidateTopologyCache();

		nv = 0;

		vb =  Point3(w,l,h)/float(2);   
		va = -vb;

#ifdef BOTTOMPIV
		va.z = float(0);
		vb.z = h;
#endif

		float dx = w/wsegs;
		float dy = l/lsegs;
		float dz = h/hsegs;

		// do bottom vertices.
		p.z = va.z;
		p.y = va.y;
		for(iy=0; iy<=lsegs; iy++) {
			p.x = va.x;
			for (ix=0; ix<=wsegs; ix++) {
				mesh.setVert(nv++, p);
				p.x += dx;
			}
			p.y += dy;
		}

		nf = 0;

		// do bottom faces.
		for(iy=0; iy<lsegs; iy++) {
			kv = iy*(wsegs+1);
			for (ix=0; ix<wsegs; ix++) {
				MAKE_QUAD(kv, kv+wsegs+1, kv+wsegs+2, kv+1, 1, bias);
				kv++;
			}
		}
		assert(nf==lsegs*wsegs*2);

		// do top vertices.
		p.z = vb.z;
		p.y = va.y;
		for(iy=0; iy<=lsegs; iy++) {
			p.x = va.x;
			for (ix=0; ix<=wsegs; ix++) {
				mesh.setVert(nv++, p);
				p.x += dx;
			}
			p.y += dy;
		}

		// do top faces (lsegs*wsegs);
		for(iy=0; iy<lsegs; iy++) {
			kv = iy*(wsegs+1)+topStart;
			for (ix=0; ix<wsegs; ix++) {
				MAKE_QUAD(kv, kv+1, kv+wsegs+2,kv+wsegs+1, 2, bias);
				kv++;
			}
		}
		assert(nf==lsegs*wsegs*4);

		// do middle vertices 
		for(iz=1; iz<hsegs; iz++) {

			p.z = va.z + dz * iz;

			// front edge
			p.x = va.x;  p.y = va.y;
			for (ix=0; ix<wsegs; ix++) { mesh.setVert(nv++, p);  p.x += dx;	}

			// right edge
			p.x = vb.x;	  p.y = va.y;
			for (iy=0; iy<lsegs; iy++) { mesh.setVert(nv++, p);  p.y += dy;	}

			// back edge
			p.x =  vb.x;  p.y =  vb.y;
			for (ix=0; ix<wsegs; ix++) { mesh.setVert(nv++, p);	 p.x -= dx;	}

			// left edge
			p.x = va.x;  p.y =  vb.y;
			for (iy=0; iy<lsegs; iy++) { mesh.setVert(nv++, p);	 p.y -= dy;	}
		}

		if (hsegs==1) {
			// do FRONT faces -----------------------
			kv = 0;
			mv = topStart;
			for (ix=0; ix<wsegs; ix++) {
				MAKE_QUAD(kv, kv+1, mv+1, mv, 3, bias);
				kv++;
				mv++;
			}

			// do RIGHT faces.-----------------------
			kv = wsegs;  
			mv = topStart + kv;
			for (iy=0; iy<lsegs; iy++) {
				MAKE_QUAD(kv, kv+wsp1, mv+wsp1, mv, 4, bias);
				kv += wsp1;
				mv += wsp1;
			}	

			// do BACK faces.-----------------------
			kv = topStart - 1;
			mv = midStart - 1;
			for (ix=0; ix<wsegs; ix++) {
				MAKE_QUAD(kv, kv-1, mv-1, mv, 5, bias);
				kv --;
				mv --;
			}

			// do LEFT faces.----------------------
			kv = lsegs*(wsegs+1);  // index into bottom
			mv = topStart + kv;
			for (iy=0; iy<lsegs; iy++) {
				MAKE_QUAD(kv, kv-wsp1, mv-wsp1, mv, 6, bias);
				kv -= wsp1;
				mv -= wsp1;
			}
		}

		else {
			// do front faces.
			kv = 0;
			mv = midStart;
			for(iz=0; iz<hsegs; iz++) {
				if (iz==hsegs-1) mv = topStart;
				for (ix=0; ix<wsegs; ix++) 
					MAKE_QUAD(kv+ix, kv+ix+1, mv+ix+1, mv+ix, 3, bias);
				kv = mv;
				mv += nlayer;
			}

			assert(nf==lsegs*wsegs*4 + wsegs*hsegs*2);

			// do RIGHT faces.-------------------------
			// RIGHT bottom row:
			kv = wsegs; // into bottom layer. 
			mv = midStart + wsegs; // first layer of mid verts


			for (iy=0; iy<lsegs; iy++) {
				MAKE_QUAD(kv, kv+wsp1, mv+1, mv, 4, bias);
				kv += wsp1;
				mv ++;
			}

			// RIGHT middle part:
			kv = midStart + wsegs; 
			for(iz=0; iz<hsegs-2; iz++) {
				mv = kv + nlayer;
				for (iy=0; iy<lsegs; iy++) {
					MAKE_QUAD(kv+iy, kv+iy+1, mv+iy+1, mv+iy, 4, bias);
				}
				kv += nlayer;
			}

			// RIGHT top row:
			kv = midStart + wsegs + (hsegs-2)*nlayer; 
			mv = topStart + wsegs;
			for (iy=0; iy<lsegs; iy++) {
				MAKE_QUAD(kv, kv+1, mv+wsp1, mv, 4, bias);
				mv += wsp1;
				kv++;
			}

			assert(nf==lsegs*wsegs*4 + wsegs*hsegs*2 + lsegs*hsegs*2);

			// do BACK faces. ---------------------
			// BACK bottom row:
			kv = topStart - 1;
			mv = midStart + wsegs + lsegs;
			for (ix=0; ix<wsegs; ix++) {
				MAKE_QUAD(kv, kv-1, mv+1, mv, 5, bias);
				kv --;
				mv ++;
			}

			// BACK middle part:
			kv = midStart + wsegs + lsegs; 
			for(iz=0; iz<hsegs-2; iz++) {
				mv = kv + nlayer;
				for (ix=0; ix<wsegs; ix++) {
					MAKE_QUAD(kv+ix, kv+ix+1, mv+ix+1, mv+ix, 5, bias);
				}
				kv += nlayer;
			}

			// BACK top row:
			kv = midStart + wsegs + lsegs + (hsegs-2)*nlayer; 
			mv = topStart + lsegs*(wsegs+1)+wsegs;
			for (ix=0; ix<wsegs; ix++) {
				MAKE_QUAD(kv, kv+1, mv-1, mv, 5, bias);
				mv --;
				kv ++;
			}

			assert(nf==lsegs*wsegs*4 + wsegs*hsegs*4 + lsegs*hsegs*2);

			// do LEFT faces. -----------------
			// LEFT bottom row:
			kv = lsegs*(wsegs+1);  // index into bottom
			mv = midStart + 2*wsegs +lsegs;
			for (iy=0; iy<lsegs; iy++) {
				nextm = mv+1;
				if (iy==lsegs-1) 
					nextm -= nlayer;
				MAKE_QUAD(kv, kv-wsp1, nextm, mv, 6, bias);
				kv -=wsp1;
				mv ++;
			}

			// LEFT middle part:
			kv = midStart + 2*wsegs + lsegs; 
			for(iz=0; iz<hsegs-2; iz++) {
				mv = kv + nlayer;
				for (iy=0; iy<lsegs; iy++) {
					nextm = mv+1;
					nextk = kv+iy+1;
					if (iy==lsegs-1) { 
						nextm -= nlayer;
						nextk -= nlayer;
					}
					MAKE_QUAD(kv+iy, nextk, nextm, mv, 6, bias);
					mv++;
				}
				kv += nlayer;
			}

			// LEFT top row:
			kv = midStart + 2*wsegs + lsegs+ (hsegs-2)*nlayer; 
			mv = topStart + lsegs*(wsegs+1);
			for (iy=0; iy<lsegs; iy++) {
				nextk = kv+1;
				if (iy==lsegs-1) 
					nextk -= nlayer;
				MAKE_QUAD(kv, nextk, mv-wsp1, mv, 6, bias);
				mv -= wsp1;
				kv++;
			}
		}

		if (genUVs) {
			int ls = lsegs+1;
			int ws = wsegs+1;
			int hs = hsegs+1;
			int ntverts = ls*hs + hs*ws + ws*ls ;
			mesh.setNumTVerts( ntverts ) ;
			mesh.setNumTVFaces(nfaces);		

			int xbase = 0;
			int ybase = ls*hs;
			int zbase = ls*hs + hs*ws;

			if (w==0.0f) w = .0001f;
			if (l==0.0f) l = .0001f;
			if (h==0.0f) h = .0001f;

			BOOL usePhysUVs = FALSE;
			float maxW = usePhysUVs ? w : 1.0f;
			float maxL = usePhysUVs ? l : 1.0f;
			float maxH = usePhysUVs ? h : 1.0f;

			float dw = maxW/float(wsegs);
			float dl = maxL/float(lsegs);
			float dh = maxH/float(hsegs);

			float u,v;

			nv = 0;
			v = 0.0f;
			// X axis face
			for (iz =0; iz<hs; iz++) {
				u = 0.0f; 
				for (iy =0; iy<ls; iy++) {
					mesh.setTVert(nv, u, v, 0.0f);
					nv++; u+=dl;
				}
				v += dh;
			}

			v = 0.0f; 
			//Y Axis face
			for (iz =0; iz<hs; iz++) {
				u = 0.0f;
				for (ix =0; ix<ws; ix++) {
					mesh.setTVert(nv, u, v, 0.0f);
					nv++; u+=dw;
				}
				v += dh;
			}

			v = 0.0f; 
			for (iy =0; iy<ls; iy++) {
				u = 0.0f; 
				for (ix =0; ix<ws; ix++) {
					mesh.setTVert(nv, u, v, 0.0f);
					nv++; u+=dw;
				}
				v += dl;
			}

			assert(nv==ntverts);

			for (nf = 0; nf<nfaces; nf++) {
				Face& f = mesh.faces[nf];
				DWORD* nv = f.getAllVerts();
				Point3 v[3];
				for (ix =0; ix<3; ix++)
					v[ix] = mesh.getVert(nv[ix]);
				int dir = direction(v);
				int ntv[3];
				for (ix=0; ix<3; ix++) {
					int iu,iv;
					switch(dir) {
					case POSX: case NEGX:
						iu = int(((float)lsegs*(v[ix].y-va.y)/l)+.5f); 
						iv = int(((float)hsegs*(v[ix].z-va.z)/h)+.5f);  
						if (dir==NEGX) iu = lsegs-iu;
						ntv[ix] = (xbase + iv*ls + iu);
						break;
					case POSY: case NEGY:
						iu = int(((float)wsegs*(v[ix].x-va.x)/w)+.5f);  
						iv = int(((float)hsegs*(v[ix].z-va.z)/h)+.5f); 
						if (dir==POSY) iu = wsegs-iu;
						ntv[ix] = (ybase + iv*ws + iu);
						break;
					case POSZ: case NEGZ:
						iu = int(((float)wsegs*(v[ix].x-va.x)/w)+.5f);  
						iv = int(((float)lsegs*(v[ix].y-va.y)/l)+.5f); 
						if (dir==NEGZ) iu = wsegs-iu;
						ntv[ix] = (zbase + iv*ws + iu);
						break;
					}
				}
				assert(ntv[0]<ntverts);
				assert(ntv[1]<ntverts);
				assert(ntv[2]<ntverts);

				mesh.tvFace[nf].setTVerts(ntv[0],ntv[1],ntv[2]);
				mesh.setFaceMtlIndex(nf,mapDir[dir]);
			}
		}
		else 
		{
			mesh.setNumTVerts(0);
			mesh.setNumTVFaces(0);
			for (nf = 0; nf<nfaces; nf++) {
				Face& f = mesh.faces[nf];
				DWORD* nv = f.getAllVerts();
				Point3 v[3];
				for (int ix =0; ix<3; ix++)
					v[ix] = mesh.getVert(nv[ix]);
				int dir = direction(v);
				mesh.setFaceMtlIndex(nf,mapDir[dir]);
			}
		}

		mesh.InvalidateTopologyCache();
	}
#include "HoudiniEngine_logo.h"
	void BuildLogoMesh(Mesh& mesh)
	{
		mesh.Init();

		mesh.setNumVerts(he_logo_verts);
		mesh.setNumFaces(he_logo_faces);
		mesh.InvalidateTopologyCache();

		const float scl = 0.1f;
		for (int v = 0; v < he_logo_verts; ++v)
		{
			Point3 p(he_logo_vert[v][0] * scl, he_logo_vert[v][1] * scl, he_logo_vert[v][2] * scl);
			mesh.setVert(v, p);
		}
		for (int f = 0; f < he_logo_faces; ++f)
		{
			mesh.faces[f].setVerts(he_logo_face[f][0] - 1, he_logo_face[f][1] - 1, he_logo_face[f][2] - 1);
		}


		mesh.InvalidateTopologyCache();
	}

	void BuildMeshFromCookResult( Mesh& mesh, HAPI_AssetId asset_id, float scl, bool forceUpdate )
	{
		HAPI_Result hstat = HAPI_RESULT_SUCCESS;
		HAPI_AssetId myAssetId( asset_id );
		HAPI_PartInfo myPartInfo;
		HAPI_AssetInfo asset_info;

		HAPI_GetAssetInfo(hapi::Engine::instance()->session(), myAssetId, &asset_info);
		if ( asset_info.objectCount )
		{
			HAPI_ObjectInfo* oinfo = new HAPI_ObjectInfo[ asset_info.objectCount ];
			HAPI_GetObjects(hapi::Engine::instance()->session(), myAssetId, oinfo, 0, asset_info.objectCount);
			bool needUpdateGeo = forceUpdate;

			if (!needUpdateGeo)
			{
				for ( int obj = 0; obj < asset_info.objectCount; obj ++ )
				{
					if ( oinfo[obj].isVisible && oinfo[obj].geoCount )
					{
						for ( int geo = 0; geo < oinfo[obj].geoCount; geo ++ )
						{
							HAPI_GeoInfo geoinfo;
							HAPI_GetGeoInfo(hapi::Engine::instance()->session(), myAssetId, oinfo[obj].id, geo, &geoinfo);

							if ( geoinfo.isDisplayGeo && geoinfo.hasGeoChanged )
							{
								needUpdateGeo = true;
								break;
							}
						}
					}
				}
			}

			if ( needUpdateGeo )
			{
				mesh.Init();
				for ( int obj = 0; obj < asset_info.objectCount; obj ++ )
				{
					if ( oinfo[obj].isVisible && oinfo[obj].geoCount )
					{
						for ( int geo = 0; geo < oinfo[obj].geoCount; geo ++ )
						{
							HAPI_GeoInfo geoinfo;
							HAPI_GetGeoInfo(hapi::Engine::instance()->session(), myAssetId, oinfo[obj].id, geo, &geoinfo);

							if ( geoinfo.isDisplayGeo )
							{
								for ( int part = 0; part < geoinfo.partCount; ++part )
								{
									hstat = HAPI_GetPartInfo(hapi::Engine::instance()->session(), myAssetId, oinfo[obj].id, geo, part, &myPartInfo);
									if ( hstat == HAPI_RESULT_SUCCESS )
									{
										if ( myPartInfo.faceCount && myPartInfo.pointCount )
										{
											HAPI_Bool are_all_the_same;
											HAPI_MaterialId* matid = new HAPI_MaterialId[myPartInfo.faceCount];
											HAPI_GetMaterialIdsOnFaces(hapi::Engine::instance()->session(),
												asset_id, oinfo[obj].id, geo, part,
												&are_all_the_same,
												matid,
												0, myPartInfo.faceCount);

											int vertOfs = mesh.getNumVerts();
											mesh.setNumVerts( vertOfs + (int)myPartInfo.pointCount, TRUE );
											mesh.InvalidateTopologyCache();
											{
												HAPI_AttributeInfo attr_info;
												attr_info.exists = false;
												Point3* v = new Point3[myPartInfo.pointCount];
												HAPI_GetAttributeInfo(hapi::Engine::instance()->session(),
													myAssetId, oinfo[obj].id, geo, part,
													"P",
													HAPI_ATTROWNER_POINT,
													&attr_info
													);
												HAPI_GetAttributeFloatData(hapi::Engine::instance()->session(),
													myAssetId, oinfo[obj].id, geo, part,
													"P",
													&attr_info,
													(float*)&v[0],
													0, attr_info.count
													);

												for ( int i = 0; i < myPartInfo.pointCount; ++i )
												{
													float y = v[i].y;
													v[i].y = -v[i].z;
													v[i].z = y;
													v[i] *= scl;
													mesh.verts[i+vertOfs] = v[i];
												}
												delete [] v;
											}
											std::vector<int> polyCount;
											std::vector<int> polyConnect;
											std::vector<int> sg;
											std::vector<int> mid;
											// polygon counts
											{
												polyCount.resize(myPartInfo.faceCount);

												if(myPartInfo.faceCount)
												{
													HAPI_GetFaceCounts(hapi::Engine::instance()->session(),
														myAssetId, oinfo[obj].id, geo, part,
														&polyCount.front(),
														0,
														myPartInfo.faceCount
														);

													HAPI_AttributeInfo attr_info;
													attr_info.exists = false;
													HAPI_GetAttributeInfo(hapi::Engine::instance()->session(),
															myAssetId, oinfo[obj].id, geo, part,
															"max_sg",
															HAPI_ATTROWNER_PRIM,
															&attr_info
															);

													if(attr_info.exists)
													{
														sg.resize(attr_info.count * attr_info.tupleSize);
														HAPI_GetAttributeIntData(hapi::Engine::instance()->session(),
																myAssetId, oinfo[obj].id, geo, part,
																"max_sg",
																&attr_info,
																&sg.front(),
																0,
																attr_info.count
																);
													}
													HAPI_GetAttributeInfo(hapi::Engine::instance()->session(),
															myAssetId, oinfo[obj].id, geo, part,
															"max_mid",
															HAPI_ATTROWNER_PRIM,
															&attr_info
															);

													if(attr_info.exists)
													{
														mid.resize(attr_info.count * attr_info.tupleSize);
														HAPI_GetAttributeIntData(hapi::Engine::instance()->session(),
																myAssetId, oinfo[obj].id, geo, part,
																"max_mid",
																&attr_info,
																&mid.front(),
																0,
																attr_info.count
																);
													}
												}
											}
											// polygon connects
											{
												polyConnect.resize(myPartInfo.vertexCount);

												if(myPartInfo.vertexCount)
												{
													HAPI_GetVertexList(hapi::Engine::instance()->session(),
														myAssetId, oinfo[obj].id, geo, part,
														&polyConnect.front(),
														0,
														myPartInfo.vertexCount
														);
												}
											}
											int faces = 0;
											{for ( int i = 0; i < myPartInfo.faceCount; ++i )
											{
												faces += polyCount[i] - 2;
											}}
											int faceOfs = mesh.getNumFaces();
											mesh.setNumFaces( faceOfs+faces, TRUE );
											mesh.InvalidateTopologyCache();

											int currentVtxIndex = 0;
											bool found_sg = sg.size() ? true : false;
											bool found_mid = mid.size() ? true : false;
											int face = faceOfs;
											for ( int i = 0; i < myPartInfo.faceCount; ++i )
											{
												int numPointsInFace = polyCount[i];
												for ( size_t j = 0; j < (numPointsInFace-2); j ++ )
												{
													mesh.faces[face].setVerts(
														polyConnect[currentVtxIndex] + vertOfs,
														polyConnect[currentVtxIndex+j+2] + vertOfs,
														polyConnect[currentVtxIndex+j+1] + vertOfs );
													if ( found_sg )
														mesh.faces[face].setSmGroup(sg[i]);
													else
														mesh.faces[face].setSmGroup(1);

													if ( found_mid )
														mesh.faces[face].setMatID((MtlID)mid[i]);
													else
													{
														if ( are_all_the_same )
															mesh.faces[face].setMatID(1);
														else
															mesh.faces[face].setMatID(matid[i]);
													}

													if ( numPointsInFace == 3 )
														mesh.faces[face].setEdgeVisFlags(1,1,1);
													else if ( j == 0 )
														mesh.faces[face].setEdgeVisFlags(0,1,1);
													else if ( j == (numPointsInFace-3) ) 
														mesh.faces[face].setEdgeVisFlags(1,1,0);
													else
														mesh.faces[face].setEdgeVisFlags(0,1,0);
													face ++;
												}
												currentVtxIndex += numPointsInFace;
											}

											// uv
											{
												HAPI_AttributeOwner owner = HAPI_ATTROWNER_MAX;

												// sill working multi uv support
												//for (int i = 1; i < MAX_MESHMAPS; ++i)
												for (int i = 1; i < 2; ++i)
												{
													std::string uvName;
													std::string uvNumberName;
													uvName = i == 1 ? std::string("uv") : std::string("uv") + std::to_string(i);
													uvNumberName = i == 1 ? std::string("uvNumber") : std::string("uvNumber") + std::to_string(i);

													int tvertOfs = mesh.getNumTVerts();
													int uvSize = 0;
													std::vector<float> uv;

													HAPI_AttributeOwner owner = HAPI_ATTROWNER_MAX;
													for (int i = 0; i < 2; i++)
													{
														// Point or Vertex
														HAPI_AttributeOwner data_type = i == 0 ? HAPI_ATTROWNER_POINT : HAPI_ATTROWNER_VERTEX;
														HAPI_AttributeInfo attr_info;
														attr_info.exists = false;
														HAPI_GetAttributeInfo(hapi::Engine::instance()->session(),
															myAssetId, oinfo[obj].id, geo, part,
															uvName.c_str(),
															data_type,
															&attr_info
															);

														if (attr_info.exists)
														{
															uvSize = attr_info.count;
															uv.resize(attr_info.count * attr_info.tupleSize);
															HAPI_GetAttributeFloatData(hapi::Engine::instance()->session(),
																myAssetId, oinfo[obj].id, geo, part,
																uvName.c_str(),
																&attr_info,
																(float*)&uv.front(),
																0, attr_info.count
																);

															owner = data_type;
															break;
														}
													}


													std::vector<int> uvNumbers;

													if (owner != HAPI_ATTROWNER_MAX)
													{
														bool useUVNumber = false;
														HAPI_AttributeInfo attr_info;
														attr_info.exists = false;
														HAPI_GetAttributeInfo(hapi::Engine::instance()->session(),
															myAssetId, oinfo[obj].id, geo, part,
															uvNumberName.c_str(),
															owner,
															&attr_info
															);

														if (attr_info.exists)
														{
															uvNumbers.resize(attr_info.count);
															HAPI_GetAttributeIntData(hapi::Engine::instance()->session(),
																myAssetId, oinfo[obj].id, geo, part,
																uvNumberName.c_str(),
																&attr_info,
																&uvNumbers.front(),
																0, attr_info.count
																);
														}
													}

													std::vector<int> vertexList(polyConnect.size());
													std::vector<float> uArray(polyConnect.size());
													std::vector<float> vArray(polyConnect.size());

													if (owner == HAPI_ATTROWNER_VERTEX && uvNumbers.size())
													{

														// attempt to restore the shared UVs

														// uvNumber -> uvIndex
														std::map<int, int> uvNumberMap;
														// uvIndex -> alternate uvIndex
														std::vector<int> uvAlternateIndexMap(
															polyConnect.size(),
															-1
															);

														int uvCount = 0;
														for (unsigned int i = 0; i < polyConnect.size(); ++i)
														{
															int uvNumber = uvNumbers[i];
															float u = uv[i * 3 + 0];
															float v = uv[i * 3 + 1];

															std::map<int, int>::iterator iter
																= uvNumberMap.find(uvNumber);

															int lastMappedUVIndex = -1;
															int mappedUVIndex = -1;
															if (iter != uvNumberMap.end())
															{
																int currMappedUVIndex = iter->second;
																while (currMappedUVIndex != -1)
																{
																	// check that the UV coordinates are the same
																	if (u == uArray[currMappedUVIndex]
																		&& v == vArray[currMappedUVIndex])
																	{
																		mappedUVIndex = currMappedUVIndex;
																		break;
																	}

																	lastMappedUVIndex = currMappedUVIndex;
																	currMappedUVIndex = uvAlternateIndexMap[currMappedUVIndex];
																}
															}

															if (mappedUVIndex == -1)
															{
																mappedUVIndex = uvCount;
																uvCount++;

																uArray[mappedUVIndex] = u;
																vArray[mappedUVIndex] = v;

																if (lastMappedUVIndex != -1)
																{
																	uvAlternateIndexMap[lastMappedUVIndex] = mappedUVIndex;
																}
																else
																{
																	uvNumberMap[uvNumber] = mappedUVIndex;
																}
															}

															vertexList[i] = mappedUVIndex;
														}

														uArray.resize(uvCount);
														vArray.resize(uvCount);
														mesh.setNumTVerts(tvertOfs + uvCount, TRUE);
														for (int v = 0; v < uvCount; v++)
														{
															mesh.tVerts[tvertOfs + v] = Point3(uArray[v], vArray[v], 0.f);
														}

														if (SetNumTVFaces(mesh, faceOfs, faceOfs + faces))
														{
															int currentVtxIndex = 0;
															int face = faceOfs;
															for (int i = 0; i < myPartInfo.faceCount; ++i)
															{
																int numPointsInFace = polyCount[i];
																for (int j = 0; j < (numPointsInFace - 2); j++)
																{
																	mesh.tvFace[face].setTVerts(
																		vertexList[currentVtxIndex] + tvertOfs,
																		vertexList[currentVtxIndex + j + 2] + tvertOfs,
																		vertexList[currentVtxIndex + j + 1] + tvertOfs);

																	face++;
																}
																currentVtxIndex += numPointsInFace;
															}
														}
													}
													else
													{
														if (!uv.size())
														{
															// add dummy UV 
															uv.push_back(0.f);
															uv.push_back(0.f);
															uv.push_back(0.f);
															uvSize = 1;
														}

														if (uv.size())
														{
															mesh.setNumTVerts(tvertOfs + uvSize, TRUE);
															for (int v = 0; v < uvSize; v++)
															{
																mesh.tVerts[tvertOfs + v] = Point3( uv[v*3+0], uv[v * 3 + 1], 0.f);
															}

															if (SetNumTVFaces(mesh, faceOfs, faceOfs + faces))
															{
																int currentVtxIndex = 0;
																int face = faceOfs;
																if (uvSize == 1)
																{
																	// set to empty uvs
																	for (int i = 0; i < myPartInfo.faceCount; ++i)
																	{
																		int numPointsInFace = polyCount[i];
																		for (int j = 0; j < (numPointsInFace - 2); j++)
																		{
																			mesh.tvFace[face].setTVerts(
																				tvertOfs,
																				tvertOfs,
																				tvertOfs);

																			face++;
																		}
																	}
																}
																else
																{
																	for (int i = 0; i < myPartInfo.faceCount; ++i)
																	{
																		int numPointsInFace = polyCount[i];
																		for (int j = 0; j < (numPointsInFace - 2); j++)
																		{
																			switch (owner)
																			{
																			case HAPI_ATTROWNER_POINT:
																				mesh.tvFace[face].setTVerts(
																					polyConnect[currentVtxIndex] + tvertOfs,
																					polyConnect[currentVtxIndex + j + 2] + tvertOfs,
																					polyConnect[currentVtxIndex + j + 1] + tvertOfs);
																				break;
																			case HAPI_ATTROWNER_VERTEX:
																				mesh.tvFace[face].setTVerts(
																					currentVtxIndex + tvertOfs,
																					currentVtxIndex + j + 2 + tvertOfs,
																					currentVtxIndex + j + 1 + tvertOfs);
																				break;
																			}

																			face++;
																		}
																		currentVtxIndex += numPointsInFace;
																	}
																}
															}
														}
													}
												}
											}
											delete[] matid;
										}
									}
								}
							}
						}
					}
				}
			}
			mesh.InvalidateTopologyCache();
			//mesh.InvalidateGeomCache();
			delete [] oinfo;
		}
	}


	Mtl* createMaxMaterial(HAPI_AssetId asset_id, HAPI_MaterialId material_id, TSTR& textureWorkPath)
	{
		HAPI_MaterialInfo materialInfo;
		HAPI_GetMaterialInfo(
			hapi::Engine::instance()->session(),
			asset_id,
			material_id,
			&materialInfo
			);

		HAPI_NodeInfo materialNodeInfo;
		HAPI_GetNodeInfo(
			hapi::Engine::instance()->session(),
			materialInfo.nodeId,
			&materialNodeInfo
			);

		if (!materialInfo.exists)
		{
			return nullptr;
		}

		std::vector<HAPI_ParmInfo> parms(materialNodeInfo.parmCount);
		HAPI_GetParameters(hapi::Engine::instance()->session(), materialInfo.nodeId, &parms[0], 0, materialNodeInfo.parmCount);

		int ambientParmIndex = FindParm(parms, "ogl_amb");
		int diffuseParmIndex = FindParm(parms, "ogl_diff");
		int alphaParmIndex = FindParm(parms, "ogl_alpha");
		int specularParmIndex = FindParm(parms, "ogl_spec");
		int texturePathSHParmIndex = FindParm(parms, "ogl_tex#", 1);
		float valueHolder[4];


		std::string materialName = GetString(materialNodeInfo.nameSH);
		StdMat *m = NewDefaultStdMat();

		m->SetName(TSTR::FromUTF8(materialName.c_str()));

		if (ambientParmIndex >= 0)
		{
			HAPI_GetParmFloatValues(hapi::Engine::instance()->session(), materialInfo.nodeId, valueHolder,
				parms[ambientParmIndex].floatValuesIndex, 3);
			m->SetAmbient(Color(valueHolder[0], valueHolder[1], valueHolder[2]), 0);
		}

		if (specularParmIndex >= 0)
		{
			HAPI_GetParmFloatValues(hapi::Engine::instance()->session(), materialInfo.nodeId, valueHolder,
				parms[specularParmIndex].floatValuesIndex, 3);
			m->SetSpecular(Color(valueHolder[0], valueHolder[1], valueHolder[2]), 0);
		}

		if (diffuseParmIndex >= 0)
		{
			HAPI_GetParmFloatValues(hapi::Engine::instance()->session(), materialInfo.nodeId, valueHolder,
				parms[diffuseParmIndex].floatValuesIndex, 3);
			m->SetDiffuse(Color(valueHolder[0], valueHolder[1], valueHolder[2]), 0);
		}

		if (alphaParmIndex >= 0)
		{
			HAPI_GetParmFloatValues(hapi::Engine::instance()->session(), materialInfo.nodeId, valueHolder,
				parms[alphaParmIndex].floatValuesIndex, 1);
			m->SetOpacity(valueHolder[0], 0);
		}

		if (texturePathSHParmIndex >= 0)
		{
			HAPI_ParmInfo texturePathParm;
			HAPI_GetParameters(hapi::Engine::instance()->session(),
				materialInfo.nodeId,
				&texturePathParm,
				texturePathSHParmIndex,
				1
				);

			int texturePathSH;
			HAPI_GetParmStringValues(hapi::Engine::instance()->session(),
				materialInfo.nodeId,
				true,
				&texturePathSH,
				texturePathParm.stringValuesIndex,
				1
				);

			std::string texturePath = GetString(texturePathSH);

			bool hasTextureSource = GetString(texturePathSH).size() > 0;
			bool canRenderTexture = false;
			if (hasTextureSource)
			{
				HAPI_Result hapiResult;

				// this could fail if texture parameter is empty
				hapiResult = HAPI_RenderTextureToImage(hapi::Engine::instance()->session(),
					asset_id,
					materialInfo.id,
					texturePathSHParmIndex
					);

				canRenderTexture = hapiResult == HAPI_RESULT_SUCCESS;
			}
			int destinationFilePathSH = 0;

			if (canRenderTexture)
			{
				// this could fail if the image planes don't exist
				HAPI_ExtractImageToFile(hapi::Engine::instance()->session(),
					asset_id,
					materialInfo.id,
					HAPI_PNG_FORMAT_NAME,
					"C A",
					CStr::FromMSTR(textureWorkPath).data(),
					NULL,
					&destinationFilePathSH
					);
			}

			if (destinationFilePathSH > 0)
			{
				std::string texturePath = GetString(destinationFilePathSH);
				//

				BitmapTex *bmt = NewDefaultBitmapTex();
				if (bmt)
				{
					bmt->SetMapName(TSTR::FromUTF8(texturePath.c_str()));
					m->SetSubTexmap(ID_DI, bmt);
					float amt = 1.0f;
					m->SetTexmapAmt(ID_DI, amt, 0);
				}
			}
		}
		return m;
	}


	Mtl* CreateMaterial( HAPI_AssetId asset_id, TSTR& textureWorkPath )
	{
		HAPI_Result hstat = HAPI_RESULT_SUCCESS;
		HAPI_AssetId myAssetId( asset_id );
		HAPI_AssetInfo asset_info;
		Mtl* maxMaterial = NULL;

		HAPI_GetAssetInfo(hapi::Engine::instance()->session(), myAssetId, &asset_info);
		if ( asset_info.objectCount )
		{
			std::map<int,Mtl*>	maxMaterials;
			HAPI_ObjectInfo* oinfo = new HAPI_ObjectInfo[ asset_info.objectCount ];
			HAPI_GetObjects(hapi::Engine::instance()->session(), myAssetId, oinfo, 0, asset_info.objectCount);
			for ( int obj = 0; obj < asset_info.objectCount; obj ++ )
			{
				if ( oinfo[obj].isVisible && oinfo[obj].geoCount )
				{
					for ( int geo = 0; geo < oinfo[obj].geoCount; geo ++ )
					{
						HAPI_GeoInfo geoinfo;
						HAPI_GetGeoInfo(hapi::Engine::instance()->session(), myAssetId, oinfo[obj].id, geo, &geoinfo);

						if ( geoinfo.isDisplayGeo )
						{
							for ( int part = 0; part < geoinfo.partCount; ++part )
							{
								HAPI_PartInfo myPartInfo;
								hstat = HAPI_GetPartInfo(hapi::Engine::instance()->session(), myAssetId, oinfo[obj].id, geo, part, &myPartInfo);
								if (hstat == HAPI_RESULT_SUCCESS)
								{
									HAPI_Bool are_all_the_same;
									HAPI_MaterialId* matid = new HAPI_MaterialId[myPartInfo.faceCount];
									HAPI_GetMaterialIdsOnFaces(hapi::Engine::instance()->session(),
										asset_id, oinfo[obj].id, geo, part,
										&are_all_the_same,
										matid,
										0, myPartInfo.faceCount);

									if (are_all_the_same)
									{
										Mtl* m = createMaxMaterial(asset_id, matid[0], textureWorkPath);
										if (m)
											maxMaterials[1] = m;
									}
									else
									{
										for (int f = 0; f < myPartInfo.faceCount; ++f)
										{
											if (maxMaterials.find(matid[f]) == maxMaterials.end())
											{
												Mtl* m = createMaxMaterial(asset_id, matid[f], textureWorkPath);
												if (m)
													maxMaterials[matid[f]] = m;
											}
										}
									}

									delete [] matid;
								}
							}
						}
					}
				}
			}
			delete [] oinfo;
			if ( maxMaterials.size() == 1 )
			{
				maxMaterial = maxMaterials.begin()->second;
			}
			else if ( maxMaterials.size() > 1 )
			{
				MultiMtl *multiMat = NewDefaultMultiMtl();
				multiMat->SetName(maxMaterials.begin()->second->GetName());
				multiMat->SetNumSubMtls(maxMaterials.rbegin()->first);

				for (std::map<int, Mtl*>::iterator ip = maxMaterials.begin(); ip != maxMaterials.end(); ++ip)
				{
					multiMat->SetSubMtl( ip->first, ip->second );
				}
				maxMaterial = multiMat;
			}
		}
		return maxMaterial;
	}

	std::string GanerateClassID(std::string& classname)
	{
		std::string classid;
		FPValue retval;
		BOOL quietErrors = TRUE;

		if (classname.size() > 0)
		{
			std::stringstream mxs;

			// find exist classid
			mxs << "classid = \"\"" << std::endl;
			mxs << "if " << classname << " != undefind do classid = \"#(0x\" + (formattedPrint " << classname << ".classid[1] format:\"08x\")  + \",0x\" + (formattedPrint " << classname << ".classid[2] format:\"08x\") + \")\"\nclassid\n";
			ExecuteMAXScriptScript(TSTR::FromACP(mxs.str().data()), quietErrors, &retval);
			if (retval.type == TYPE_STRING)
			{
				classid = CStr::FromMCHAR(retval.s).data();
			}
		}

		if (classid.size() == 0)
		{
			ExecuteMAXScriptScript(_T("id = genClassID returnValue:True\n\"#(0x\" + (formattedPrint id[1] format:\"08x\") + \",0x\" + (formattedPrint id[2] format:\"08x\") + \")\""), quietErrors, &retval);

			if (retval.type == TYPE_STRING)
			{
				classid = CStr::FromMCHAR(retval.s).data();
			}
		}
		return classid;

	}


	std::string GetProfileString(const MCHAR* key)
	{
		std::string result;

		const MCHAR* path = IPathConfigMgr::GetPathConfigMgr()->GetDir(APP_PLUGCFG_DIR);
		if (path)
		{
			const size_t cleng = 1024;
			WCHAR _result[cleng];
			GetPrivateProfileString(_T("HoudiniEngine"), key, _T(""), &_result[0], cleng, path);

			result = CStr::FromMCHAR(_result).data();
		}

		return result;

	}
	
	int GetProfileInt(const MCHAR* key)
	{
		int result = -1;

		const MCHAR* path = IPathConfigMgr::GetPathConfigMgr()->GetDir(APP_PLUGCFG_DIR);
		if (path)
		{
			result = GetPrivateProfileInt(_T("HoudiniEngine"), key, -1, path);
		}

		return result;

	}
};