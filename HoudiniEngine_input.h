#ifndef __HOUDINI_ENGINE_INPUT__
#define  __HOUDINI_ENGINE_INPUT__

#include <vector>

class INode;
struct InputAsset
{
	InputAsset() : node(nullptr), asset_id(-1) {}
	INode*	node;
	int		asset_id;
};

int InputNode( int asset_id, int input_id, INode* node, TimeValue t, Matrix3 &baseTM, double scale, InputAsset* iasset = NULL);

class InputAssets
{
public:
	InputAssets();
	~InputAssets();
	void setAssetId( int asset_id );
	bool setNode( int ch, INode* node, TimeValue t, Matrix3 &baseTM, double scale, bool check_v_update = false );
	void disconnect( int ch, bool free_node = true );
	void release();
	int getAssetId() { return assetId;  }
	size_t getNumInputs() { return inputs.size(); }
	INode* getINode(int ch) { return inputs[ch].node; }
private:
	std::vector<InputAsset>		inputs;
	int							assetId;
	HAPI_AssetInfo				assetInfo;
};

#endif // __HOUDINI_ENGINE_INPUT__