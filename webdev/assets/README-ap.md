This folder contains webpages and JS/CSS assets hosted on AP interface of ESP32. These pages can only be accessed when your computer is connected to ESP32's Access Point as a station, which means these features -- Files Manager, Online Files Editor, Configuration -- are protected from global network (e.g. IoT cloud services). 

The Files Manager page shows information of ESP32's SPI flash filesystem partition, letting user to view/upload/delete files or folders.

Online Editor is implemented at /ap/editor.html and can be invoked by:
1. Visit http://{HOSTNAME}/ap/editor.html?filename=/path/to/file
    * Additional parameters can be: `?theme=textmate&lang=html`
2. Click on the edit button after specific filename in Files Manager page.

The `js/file-api.js` provides an abstraction layer on backend interfaces, which should be modified when WebServer's APIs change. It is used by both Online Editor and Files Manager.

File `js/config.js` is used by Configuration page to interchange config information between browser and ESP32.

Some files are renamed because of limitation on filename length of SPIFFS.

## Files for building ACE Editor
- js/ace.js
- js/ext-shortcut.js (ext-keybinding_menu.js)
- js/ext-searchbox.js
- js/ext-settings.js (ext-settings_menu.js)

## Language hightlight
- js/mode-css.js
- js/mode-gcode.js
- js/mode-html.js
- js/mode-ini.js
- js/mode-javascript.js

## Themes (light & dark)
- js/theme-textmate.js
- js/theme-twilight.js
