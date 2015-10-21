
rollout HoudiniEngineCreateMeshPlugin "HoudiniEngine Create Mesh Plugin" width:500 height:280
(
    local inifile = getDir #plugcfg + "\HoudiniEngine.ini"
    group "Mesh Plugin"
    (
        label k1 "Asset(otl,hda):" align:#left
        edittext asset_path "" fieldWidth:440 height:15 height:15 across:2
        button asset_path_load "..." align:#right
        label l2 "Name:" align:#left
        edittext asset_name "" fieldWidth:440 height:15 height:15
        label l3 "Category:" align:#left
        edittext category_name "" fieldWidth:440 height:15 height:15 text:"HoudiniEngine"
        label l4 "HoudiniEngne Plugin Path:" align:#left
        edittext plugin_path "" fieldWidth:440 height:15 height:15 across:2
    --    button plugin_path_load "..." align:#right
        label l5 "Texture Path:" align:#left
        edittext texture_path "" fieldWidth:440 height:15 height:15 across:2
    --    button texture_path_load "..." align:#right
    )
    
    button okButton  "Ok" pos:[300+64,240] width:64 height:24
    button cancelButton  "Cancel" pos:[300+64+64,240] width:64 height:24
    
    fn load_settings =
    (
        local s_texture_path = GetINISetting inifile "HoudiniEngine" "texture_path"
        local s_plugin_path = GetINISetting inifile "HoudiniEngine" "plugin_path"
        
        if s_plugin_path == undefined or s_plugin_path.count == 0 do
        (
            return false
        )
        plugin_path.text = s_plugin_path
        texture_path.text = s_texture_path
        return true
    )
    
    fn get_directory initpath =
    (
        return getSavePath caption:"Select Search Path" initialDir:initpath
    )
    
    fn create_plugin = 
    (
        try
        (
            local filename = ( plugin_path.text+"\\"+asset_name.text+".ms")
            local pluginname = execute(asset_name.text)
			
            if pluginname != undefined do
            (
                if querybox (asset_name.text+" plugins already exists. Do you want to continue?") title:"HoudiniEngine" == false do
				(
					return false
				)
            )
            HoudiniEngine.initialize()
            print asset_path.text
            print asset_name.text
            print category_name.text
            print texture_path.text
            print filename
            local result = HoudiniEngine.generatePluginScript  asset_path.text asset_name.text category_name.text texture_path.text filename
            print result
            
            if result == true then
            (
                fileIn filename
                messageBox ("HoudiniEngine plugin has been created\n"+filename) title:"HoudiniEngine" beep:false
                return true
            )
        )
        catch
        (
            messageBox ("error") title:"HoudiniEngine" beep:false
        )
        return false
    )
    
    on HoudiniEngineCreateMeshPlugin open do
    (
        format ("ini file location = " + inifile)
        if load_settings() == false do
        (
            destroyDialog HoudiniEngineCreateMeshPlugin
            messageBox "Please HoudiniEngine Setting first." title:"HoudiniEngine" beep:false
        )
    )
    
    on asset_path_load pressed do
    (
        local filename = getOpenFileName caption:"Import Digital Asset:" types:"Houdini Digital Asset (*.otl;*.hda)|*.otl;*.hda|All(*.*)|*.*" historyCategory:"HoudiniEngine"
        if (filename != undefined) do
        (
            asset_path.text = filename
            
            if asset_name.text.count == 0 do
            (
                asset_name.text = getFilenameFile filename
            )
        )
    )
    on okButton pressed do
    (
        local param_size = asset_path.text.count * asset_name.text.count * category_name.text.count * plugin_path.text.count *texture_path.text.count
        if param_size  == 0 then
        (
            messageBox "Please set parameters." title:"HoudiniEngine" beep:false
        )
        else
        (
            local result = create_plugin()
            if result == true do
            (
                destroyDialog HoudiniEngineCreateMeshPlugin
            )
        )
    )
    on cancelButton pressed do
    (
        destroyDialog HoudiniEngineCreateMeshPlugin
    )
)

fn HoudiniEngineCreateMeshPluginDialog =
(
    createDialog HoudiniEngineCreateMeshPlugin
)


