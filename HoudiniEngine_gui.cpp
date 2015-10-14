#include "HoudiniEngine.h"
#include "HoudiniEngine_util.h"
#include "HoudiniEngine_gui.h"
#include <maxscript\maxscript.h>
#include <sstream>

#define ECR		<<"\n";
#define SYNC_PARAMS		"if this.delegate.autoupdate do this.delegate.autoupdate = True"

static bool GetParmParentVisible( std::vector<hapi::Parm>& parms, int id )
{
	if ( id < 0 )
		return false;

	for (int i=0; i < int(parms.size()); ++i)
	{
		hapi::Parm &parm = parms[i];
		if ( parm.info().id == id )
		{
			if ( parm.info().invisible )
				return true;

			if ( parm.info().parentId >= 0 && GetParmParentVisible( parms, parm.info().parentId ) )
				return true;
		}
	}
	return false;
}


bool GenerateScriptPlugin(std::string& otl_path, std::string& name, std::string& category, std::string& texture_path, std::string& classid, std::string& code )
{
	int asset_id = -1;
	hapi::Engine* engine = hapi::Engine::instance();
	if (engine)
	{
		// loading asset
		int library_id = engine->loadAssetLibrary(otl_path.c_str());
		if (library_id >= 0)
		{
			std::string asset_name = engine->getAssetName(library_id, 0);
			asset_id = engine->instantiateAsset(asset_name.c_str(), false);
		}
	}

	hapi::Asset asset(asset_id);
	if (asset.isValid())
	{
		std::stringstream mxs;

		mxs << "plugin geometry " << name ECR
		mxs << "name:\"" << name << "\"" ECR
		mxs << "extends:HEMesh" ECR
		mxs << "classID:" << classid ECR
		mxs << "category:\"" << category << "\"" ECR
		mxs << "(" ECR
			// Inputs
			if (asset.info().geoInputCount)
			{
				const int getInputs = asset.info().geoInputCount;
				mxs << "\tparameters pblock rollout:inputs" ECR
				mxs << "\t(" ECR
				for (int i = 0; i < getInputs; ++i)
				{
					mxs << "\t\t __he_input" << i << " type:#node ui:__he_input" << i ECR
				}
				mxs << "\t)" ECR
				mxs << "\trollout inputs \"Inputs\"" ECR
				mxs << "\t(" ECR
				for (int i = 0; i < getInputs; ++i)
				{
					std::string inputname = asset.getInputName(i);
					mxs << "\t\t" << "label label_he_input" << i << " \"" << inputname << "\" width:140" ECR
					mxs << "\t\t" << "pickbutton __he_input" << i << " \"" << "Pick Node" << "\" width:140" ECR
				}
				for (int i = 0; i < getInputs; ++i)
				{
					mxs << "\t\ton __he_input" << i << " picked obj do (\n\t\t\t__he_input" << i << ".text=obj.name\n\t\t\t" SYNC_PARAMS "\n\t\t)" ECR
				}
				mxs << "\t\ton inputs open do\n\t\t(" ECR
				for (int i = 0; i < getInputs; ++i)
				{
					mxs << "\t\t\tif __he_input" << i << ".object != undefined do __he_input" << i << ".text = __he_input" << i << ".object.name" ECR
				}
				mxs << "\t\t)" ECR
				mxs << "\t)" ECR

			}
			mxs << "\tparameters pblock rollout:params" ECR
			mxs << "\t(" ECR
			// Parameters
			std::vector<hapi::Parm> _parms = asset.parms();
			for (int i = 0; i < int(_parms.size()); ++i)
			{
				hapi::Parm &parm = _parms[i];
				if (parm.info().invisible || GetParmParentVisible(_parms, parm.info().parentId))
					continue;

				if (parm.info().type == HAPI_PARMTYPE_TOGGLE)
				{
					std::string name = parm.name();
					int value = parm.getIntValue(0);
					mxs << "\t\t" << name << "0 type:#integer ui:" << name << "0 default:" << value ECR
				}
				else if (HAPI_ParmInfo_IsInt(&parm.info()))
				{
					std::string name = parm.name();
					for (int sub_index = 0; sub_index < parm.info().size; ++sub_index)
					{
						int value = parm.getIntValue(sub_index);
						mxs << "\t\t" << name << sub_index << " type:#integer animatable:true ui:" << name << sub_index << " default:" << value ECR
					}
				}
				else if (HAPI_ParmInfo_IsFloat(&parm.info()))
				{
					std::string name = parm.name();
					for (int sub_index = 0; sub_index < parm.info().size; ++sub_index)
					{
						float value = parm.getFloatValue(sub_index);
						mxs << "\t\t" << name << sub_index << " type:#float animatable:true ui:" << name << sub_index << " default:" << value ECR
					}
				}
				else if (HAPI_ParmInfo_IsString(&parm.info()))
				{
					std::string name = parm.name();
					for (int sub_index = 0; sub_index < parm.info().size; ++sub_index)
					{
						std::string value = parm.getStringValue(sub_index);
						mxs << "\t\t" << name << sub_index << " type:#string ui:" << name << sub_index << " default:\"" << value << "\"" ECR
					}
				}
			}
			mxs << "\t)" ECR
			mxs << "\trollout params \"Parameters\"" ECR
			mxs << "\t(" ECR
			for (int i = 0; i < int(_parms.size()); ++i)
			{
				hapi::Parm &parm = _parms[i];
				if (parm.info().invisible || GetParmParentVisible(_parms, parm.info().parentId))
					continue;

				std::string label = parm.label();

				if (parm.info().type == HAPI_PARMTYPE_TOGGLE)
				{
					std::string name = parm.name();
					mxs << "\t\t" << "checkbox " << name << "0 \"" << label << "\"" ECR
				}
				else if (HAPI_ParmInfo_IsInt(&parm.info()))
				{
					std::string name = parm.name();
					for (int sub_index = 0; sub_index < parm.info().size; ++sub_index)
					{
						if (sub_index == 0)
							mxs << "\t\t" << "spinner " << name << sub_index << " \"" << label << "\" type:#integer" ECR
						else
							mxs << "\t\t" << "spinner " << name << sub_index << " \"\" type:#integer" ECR
					}
				}
				else if (HAPI_ParmInfo_IsFloat(&parm.info()))
				{
					std::string name = parm.name();
					for (int sub_index = 0; sub_index < parm.info().size; ++sub_index)
					{
						if (sub_index == 0)
							mxs << "\t\t" << "spinner " << name << sub_index << " \"" << label << "\" type:#float" ECR
						else
							mxs << "\t\t" << "spinner " << name << sub_index << " \"\" type:#float" ECR
					}
				}
				else if (HAPI_ParmInfo_IsString(&parm.info()))
				{
					std::string name = parm.name();
					for (int sub_index = 0; sub_index < parm.info().size; ++sub_index)
					{
						if (sub_index == 0)
							mxs << "\t\t" << "edittext " << name << sub_index << " \"" << label << "\" fieldWidth:140 labelOnTop:true" ECR
						else
							mxs << "\t\t" << "edittext " << name << sub_index << " fieldWidth:140 labelOnTop:false" ECR
					}
				}
			}
			for (int i = 0; i < int(_parms.size()); ++i)
			{
				hapi::Parm &parm = _parms[i];
				if (parm.info().invisible || GetParmParentVisible(_parms, parm.info().parentId))
					continue;

				std::string label = parm.label();

				if (parm.info().type == HAPI_PARMTYPE_TOGGLE)
				{
					std::string name = parm.name();
					mxs << "\t\ton " << name << "0 changed val do " SYNC_PARAMS ECR
				}
				else if (HAPI_ParmInfo_IsInt(&parm.info()))
				{
					std::string name = parm.name();
					for (int sub_index = 0; sub_index < parm.info().size; ++sub_index)
					{
						mxs << "\t\ton " << name << sub_index << " changed val do " SYNC_PARAMS ECR
					}
				}
				else if (HAPI_ParmInfo_IsFloat(&parm.info()))
				{
					std::string name = parm.name();
					for (int sub_index = 0; sub_index < parm.info().size; ++sub_index)
					{
						mxs << "\t\ton " << name << sub_index << " changed val do " SYNC_PARAMS ECR
					}
				}
				else if (HAPI_ParmInfo_IsString(&parm.info()))
				{
					std::string name = parm.name();
					for (int sub_index = 0; sub_index < parm.info().size; ++sub_index)
					{
						mxs << "\t\ton " << name << sub_index << " entered val do " SYNC_PARAMS ECR
					}
				}
			}
			mxs << "\t)" ECR
			mxs << "\ton create do" ECR
			mxs << "\t(" ECR
			mxs << "\t\tthis.delegate.filename = @\"" << otl_path << "\"" ECR
			mxs << "\t\tthis.delegate.update_time = true" ECR
			mxs << "\t\tthis.delegate.conv_unit_scale_i = true" ECR
			mxs << "\t\tthis.delegate.conv_unit_scale_o = true" ECR
			mxs << "\t\tthis.delegate.texture_path = @\"" << texture_path << "\"" ECR
			mxs << "\t)" ECR

		mxs << ")" ECR
		code = mxs.str();
		// Release Asset
		engine->destroyAsset(asset_id);

		return true;
	}
	return false;
}

