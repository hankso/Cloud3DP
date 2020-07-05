# Installation

First we need to install packages used in web pages. Some are included directly in HTML to construct GUI and WebApps like FontAwesome, Vue.js etc. And some packages are used to generate webpages automatically.

```bash
cd webdev
npm install
```

#### TaoBao NPM Mirror

You can either run
```bash
npm --registry=https://registry.npm.taobao.org --cache=$HOME/.npm/.cache/cnpm --disturl=https://npm.taobao.org/dist install
```

or set an alias in *~/.bashrc*

```bash
alias cnpm='npm --registry=https://registry.npm.taobao.org --cache=$HOME/.npm/.cache/cnpm --disturl=https://npm.taobao.org/dist'
cnpm install
```

to faster your installation.

?> We highly suggest you to use TaoBao mirror if your're in China. Command `cnpm install` only costs about 8 seconds to install all packages on the author's computer.

# Build
#### Web Pages
`Gulp` is employed for automate webpages generation, e.g. compile SASS files to CSS and embed them into HTMLs, move & rename files to destination according to *webdev/movefiles.json* etc.

After you run `npm install`, Gulp is available by `npx gulp`.

```bash
npx gulp build
```

or call npm scripts

```bash
npm run build
```

#### SPIFFS & FATFS
To burn webpages into ESP32's flash, you must pack all files in a binary filesystem.

```bash
make fs     # makeEspArduino
make spiffs # native esp-idf make
```

# Notes

#### About using FontAwesome with Vue.js
- Remember to include `/assets/fa-config.js` before using `fontawesome.min.js`

- With our fa-config, fa will insert a SVG child into every icon container elements. If class names like `fas` or `fab` are in the binded class list (e.g. `:class="['fas', 'fa-' + icon]"`) of the container element, every attributes will be copied to its generated SVG child element. So if you have an event callback attribute and you click on the generated SVG icon, both icon and the container element will be clicked, thus invoking callback twice. To avoid this, you can either configure fontawesome's behaviour or wrap your icons in nested elements:

    ```html
    <script inline src="../assets/fa-config.js"></script>
    <script src="/assets/js/fontawesome.min.js"></script>
    ...
    <button onclick="callback">
        <i class="fas fa-user"></i>
    </button>
    ```

#### About ESPAsyncWebServer & ESPAsyncTCP
- In ESPAsyncWebServer, class `AsyncWebSocketClient` define in *AsyncWebSocket.h* has a private member `AwsFrameInfo _pinfo`. This structure instance should be initialized as zero by `AwsFrameInfo _pino = { 0 }` or `_pinfo.num = 0`. Otherwise, `_pinfo.num` is left as an unspecified value (random number).

- In *AsyncTCP.h*, you should include `FreeRTOS.h` before including `semphr.h`, or the compiler will complain error.

```c++
extern "C" {
    #include "freertos/FreeRTOS.h" // add this line
    #include "freertos/semphr.h"
}
```
