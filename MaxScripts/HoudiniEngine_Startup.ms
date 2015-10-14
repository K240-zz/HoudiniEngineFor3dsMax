include "HoudiniEngine_Menu.mcr"

fn HoudiniEngineMenus =
(
    -- get the main menu bar
    local mainMenuBar = menuMan.getMainMenuBar()
    -- check the exists menu
    local subMenu  = menuMan.findMenu "HoudiniEngine"
        
    if (subMenu  != undefined) do 
    (
        menuMan.unRegisterMenu subMenu 
    )
    -- create a new menu
    subMenu  = menuMan.createMenu "HoudiniEngine"
    -- create a menu item 
    local pluginMenuItem = menuMan.createActionItem "HoudiniEngineCreateMeshPluginUI" "HoudiniEngine"
    local settingsMenuItem = menuMan.createActionItem "HoudiniEngineSettingsUI" "HoudiniEngine"

    -- add the item to the menu
    subMenu.addItem pluginMenuItem -1
    subMenu.addItem settingsMenuItem -1
    -- create a new menu item with the menu as it's sub-menu
    local heMainMenuItem = menuMan.createSubMenuItem "HoudiniEngine" subMenu 
    -- compute the index of the next-to-last menu item in the main menu bar
    local heMenuItemIndex = mainMenuBar.numItems()
    -- add the sub-menu just at the second to last slot
    mainMenuBar.addItem heMainMenuItem heMenuItemIndex
    -- redraw the menu bar with the new item
    menuMan.updateMenuBar()
)

HoudiniEngineMenus()
