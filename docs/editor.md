# Online Editor
In firmware Cloud3DP implemented some APIs to **list / edit / create / delete** files or directories in flash file system. It also integrates an online text editor based on project [Ace.js](https://ace.c9.io), which is released under the BSD license.

?> For security concerns, editor page is only accessable when your device connects to ESP32's SoftAP as a station/client, which means this feature is protected from global networks.

Online Editor can be opened by:
- Click on the edit button after filenames listed in [Files Manager](manager) page.
- Visit url `http://{HOSTNAME}/ap/editor.html`. Hostname is **10.0.0.1** (*config::net.ap.host*) or **cloud3dp.local** (*config::app.dns.host*). You can specify additional parameters to the url. See more at [API Reference](refer).
    + `?filename=/path/to/file`
    + `?theme=textmate`
    + `?lang=html`

#### Editor support files

```
/ap
|-- editor.html
`-- js
    |-- ace.js
    |-- ext-searchbox.js
    |-- ext-shortcut.js (ACE: ext-keybinding_menu.js)
    |-- ext-settings.js (ACE: ext-settings_menu.js)
    |-- mode-javascript.js (language hightlight)
    |-- mode-css.js
    |-- mode-html.js
    |-- mode-gcode.js
    |-- theme-textmate.js (default light & dark themes)
    `-- theme-twilight.js
```

> Some files are renamed because of limitation on filename length of SPIFFS.
