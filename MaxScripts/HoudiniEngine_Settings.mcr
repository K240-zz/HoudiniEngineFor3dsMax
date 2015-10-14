
rollout HoudiniEngineSettings "HoudiniEngine Settings" width:500 height:550
(
    local inifile = getDir #plugcfg + "\HoudiniEngine.ini"
    group "Path"
    (
        label k1 "otl Search Path:" align:#left
        edittext otl_search_path "" fieldWidth:440 height:15 across:2
        button otl_search_path_load "..." align:#right
        label l2 "dso Search Path:" align:#left
        edittext dso_search_path "" fieldWidth:440 height:15 across:2
        button dso_search_path_load "..." align:#right
        label l3 "Image dso Search Path:" align:#left
        edittext image_dso_search_path "" fieldWidth:440 height:15 across:2
        button image_dso_search_path_load "..." align:#right
        label l4 "Audio dso Search Path:" align:#left
        edittext audio_dso_search_path "" fieldWidth:440 height:15 across:2
        button audio_dso_search_path_load "..." align:#right
        label l5 "Texture Path:" align:#left
        edittext texture_path "" fieldWidth:440 height:15 across:2
        button texture_path_load "..." align:#right
    )
    group "Plugin Path"
    (
        label k11 "HoudiniEngne Plugin Path:" align:#left
        edittext plugin_path "" fieldWidth:440 height:15 across:2
        button plugin_path_load "..." align:#right
    )
	group "Cooking Mode"
	(
        checkbox multiThreadCheckbox "Multi Threading" width:200 height:15 checked:true
	)
	
	group "Session Mode"
	(
        radiobuttons  proc_mode labels:#("In Process", "Thrift Socket", "Thrift Pipe")
	)
	group "Thrift Socket Options(Session Mode)"
	(
		edittext ts_address "    Address:" fieldWidth:170 height:15 across:2
		edittext ts_port "Port:" fieldWidth:50 height:15
	)
	group "Thrift Pipe Options(Session Mode)"
	(
		edittext tp_name "    Pipe Name:" fieldWidth:170 height:15
	)
    
    
    button okButton  "Ok" pos:[300,510] width:64 height:24
    button applyButton  "Apply" pos:[300+64,510] width:64 height:24
    button cancelButton  "Cancel" pos:[300+64+64,510] width:64 height:24
    
    fn load_settings =
    (
        local s_otl_search_path = GetINISetting inifile "HoudiniEngine" "otl_search_path"
        local s_dso_search_path = GetINISetting inifile "HoudiniEngine" "dso_search_path"
        local s_image_dso_search_path = GetINISetting inifile "HoudiniEngine" "image_dso_search_path"
        local s_audio_dso_search_path = GetINISetting inifile "HoudiniEngine" "audio_dso_search_path"
        local s_texture_path = GetINISetting inifile "HoudiniEngine" "texture_path"
        local b_multiThreading = execute(GetINISetting inifile "HoudiniEngine" "multi_threading")
        local s_plugin_path = GetINISetting inifile "HoudiniEngine" "plugin_path"
		local i_proc_mode = (GetINISetting inifile "HoudiniEngine" "proc_mode") as Integer
        local s_thrift_address = GetINISetting inifile "HoudiniEngine" "thriftsocket_address"
        local s_thrift_port = GetINISetting inifile "HoudiniEngine" "thriftsocket_port"
        local s_thrift_name = GetINISetting inifile "HoudiniEngine" "thriftpipe_name"
		
        if b_multiThreading == OK do b_multiThreading = True
		
		if i_proc_mode == undefined or i_proc_mode < 1 or i_proc_mode > 3 do i_proc_mode = 1
		
        if s_plugin_path == undefined or s_plugin_path.count == 0 do
        (
            -- find houdiniengine asset folder
            for i = 1 to pluginPaths.count() do
            (
                if findString (pluginPaths.get i) "HoudiniEngineAsset" != undefined do
                (
                    print "found plugin path at plugins path\n"
                    s_plugin_path = pluginPaths.get i
                    break
                )
            )
        )
        
        otl_search_path.text = s_otl_search_path
        dso_search_path.text = s_dso_search_path
        image_dso_search_path.text = s_image_dso_search_path
        audio_dso_search_path.text = s_audio_dso_search_path
        texture_path.text = s_texture_path
        multiThreadCheckbox.checked = b_multiThreading
        plugin_path.text = s_plugin_path
		proc_mode.state = i_proc_mode
		ts_address.text = s_thrift_address
		ts_port.text = s_thrift_port
		tp_name.text = s_thrift_name
    )
	
    fn save_settings =
    (
        setINISetting inifile "HoudiniEngine" "otl_search_path" otl_search_path.text
        setINISetting inifile "HoudiniEngine" "dso_search_path" dso_search_path.text
        setINISetting inifile "HoudiniEngine" "image_dso_search_path" image_dso_search_path.text
        setINISetting inifile "HoudiniEngine" "audio_dso_search_path" audio_dso_search_path.text
        setINISetting inifile "HoudiniEngine" "texture_path" texture_path.text
        setINISetting inifile "HoudiniEngine" "multi_threading"(multiThreadCheckbox.checked as String)
        setINISetting inifile "HoudiniEngine" "plugin_path" plugin_path.text
        setINISetting inifile "HoudiniEngine" "proc_mode" (proc_mode.state as String)
        setINISetting inifile "HoudiniEngine" "thriftsocket_address" ts_address.text
        setINISetting inifile "HoudiniEngine" "thriftsocket_port" ts_port.text
        setINISetting inifile "HoudiniEngine" "thriftpipe_name" tp_name.text
    )
    
    fn get_directory initpath =
    (
        return getSavePath caption:"Select Search Path" initialDir:initpath
    )
    
    on HoudiniEngineSettings open do
    (
        format ("ini file location = " + inifile+"\n")
        load_settings()
    )
    
    on otl_search_path_load pressed do
    (
        local search_path = get_directory otl_search_path.text
        if search_path != undefined do
        (
            otl_search_path.text = search_path
        )
    )
    on dso_search_path_load pressed do
    (
        local search_path = get_directory dso_search_path.text
        if search_path != undefined do
        (
            dso_search_path.text = search_path
        )
    )
    on image_dso_search_path_load pressed do
    (
        local search_path = get_directory image_dso_search_path.text
        if search_path != undefined do
        (
            image_dso_search_path.text = search_path
        )
    )
    on audio_dso_search_path_load pressed do
    (
        local search_path = get_directory audio_dso_search_path.text
        if search_path != undefined do
        (
            audio_dso_search_path.text = search_path
        )
    )
    on texture_path_load pressed do
    (
        local search_path = get_directory texture_path.text
        if search_path != undefined do
        (
            texture_path.text = search_path
        )
    )
    on plugin_path_load pressed do
    (
        local search_path = get_directory plugin_path.text
        if search_path != undefined do
        (
            plugin_path.text = search_path
        )
    )
    on okButton pressed do
    (
        save_settings()
        destroyDialog HoudiniEngineSettings
    )
    on applyButton pressed do
    (
        save_settings()
    )
    on cancelButton pressed do
    (
        destroyDialog HoudiniEngineSettings
    )
)

fn HoudiniEngineSettingsDialog =
(
    createDialog HoudiniEngineSettings
)
