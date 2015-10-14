include "HoudiniEngine_Settings.mcr"
include "HoudiniEngine_CreatePlugin.mcr"

macroScript HoudiniEngineSettingsUI
    category:"HoudiniEngine" 
    buttonText:"HoudiniEngine Settings"
    tooltip:"HoudiniEngine Settings"
(
    on execute do  
    (
        HoudiniEngineSettingsDialog()
    )
)

macroScript HoudiniEngineCreateMeshPluginUI
    category:"HoudiniEngine" 
    buttonText:"Create HoudiniEngine Mesh Plugin"
    tooltip:"Create HoudiniEngine Mesh Plugin"
(
    on execute do  
    (
        HoudiniEngineCreateMeshPluginDialog()
    )
)
