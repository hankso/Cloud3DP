/* ESP32 Configuration module accepts flattened json
 * e.g.:
 *  source => {
 *      'a.b.c': 'dead',
 *      'e.d': {
 *          'f': 'beef'
 *      }
 *  }
 *  unflatten => {
 *      'a': {
 *          'b': {
 *              'c': 'dead'
 *          }
 *      },
 *      'e': {
 *          'd': {
 *              'f': 'beef'
 *          }
 *      }
 *  }
 *  flatten => { 'a.b.c': 'dead', 'e.d.f': 'beef' }
 */


/* ESP32 also use string '0' and '1' as boolean `false` and `true`.
 * Convertion should happen before unflattening and after flattening.
 */
function fakeboolEnter(obj) {
    let fakebools = window.fakebools = (window.fakebools || []), data = {};
    Object.entries(obj).forEach(([key, val]) => {
        if (typeof val == 'string' && ['0', '1'].includes(val)) {
            if (!fakebools.includes(key)) {
                fakebools.push(key);
            }
            data[key] = val == '1';
        } else {
            data[key] = val;
        }
    });
    return data;
}

function fakeboolExit(obj) {
    let fakebools = window.fakebools = (window.fakebools || []), data = {};
    Object.entries(obj).forEach(([key, val]) => {
        data[key] = fakebools.includes(key) ? (val ? '1' : '0') : val;
    });
    return data;
}

Object.flatten = (data) => {
    let obj = {};
    (function recurse(cur, prop) {
        if (Object(cur) !== cur) {
            obj[prop] = cur;
        } else if (Array.isArray(cur)) {
            for (var i = 0, l = cur.length; i < l; i++) {
                recurse(cur[i], `${prop}[${i}]`);
            }
            if (l == 0) obj[prop] = [];
        } else {
            let isEmpty = true;
            for (let p in cur) {
                isEmpty = false;
                recurse(cur[p], prop ? `${prop}.${p}` : p);
            }
            if (isEmpty && prop) obj[prop] = {};
        }
    })(data, '');
    return fakeboolExit(obj);
};

Object.unflatten = (data) => {
    "use strict";
    if (Object(data) !== data || Array.isArray(data))
        return data;
    data = fakeboolEnter(data);
    let regex = /\.?([^.\[\]]+)|\[(\d+)\]/g, obj = {};
    for (let p in data) {
        let cur = obj, prop = '', m;
        while (m = regex.exec(p)) {
            cur = cur[prop] || (cur[prop] = (m[2] ? [] : {}));
            prop = m[2] || m[1];
        }
        cur[prop] = data[p];
    }
    return obj[''] || obj;
};

function configInit(cfgs) {
    // `cfgs` is already flattened by the backend.
    // Convert once between to extract info of fakebools.
    // Then save raw configs in namespace.
    window.configs = Object.flatten(Object.unflatten(cfgs));
}

function configLoad() {
    let configs = Object.unflatten(window.configs),
        deepcopy = JSON.parse(JSON.stringify(configs));
    return deepcopy;
}

function configSave(cfgs) {
    let json = JSON.stringify(Object.flatten(cfgs));
    if (json == JSON.stringify(window.configs)) {
        return alert('No configurations changed. Abort!');
    }
    ajaxXHR({
        url: '/config', method: 'POST',
        data: { json: json },
        success: () => {
            let btn = document.getElementById('reload');
            if (confirm('Configuration saved! Reload?')) {
                btn && btn.click();
            } else {
                btn && (btn.style.color = 'red');
            }
        }
    });
}
