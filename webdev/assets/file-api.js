var ge = id => document.getElementById(id);
var ce = tag => document.createElement(tag);

var API = (function () {
    var apilist = {
        get:  "/edit",
        list: "/edit?list=${path}",
        edit: "/edit?path=${path}",
        down: "/edit?path=${path}&download",
        create: "/editc?path=${path}&type=${arg}",
        delete: "/editd?path=${path}",
        upload: "/editu?overwrite",
    };
    function merge(opt1, opt2) {
        if (opt2 instanceof Function) opt2 = { success: opt2 };
        ajaxXHR(Object.assign(opt1, opt2));
    }
    function url(method, path, arg) {
        if (!method in apilist) {
            console.error(`Invalid API name: "${method}"`);
            return '';
        } else
            return eval('`' + apilist[method] + '`');
    }
    return {
        url: url,
        download: (path, opt) => merge({ url: url('list', path) }, opt),
        listDir: (path, opt) => merge({
            url: url('list', path), dataType: 'json' }, opt),
        createPath: (path, isdir=true, opt={}) => merge({
            url: url('create', path, isdir ? 'folder' : 'file'),
            // method: 'PUT',
            success: () => {
                console.log(`"${path}" create success`);
                location.reload();
            },
            error: (x, c, ret) => alert(`Create "${path}" failed: ${ret}`)
        }, opt),
        deletePath: (path, opt) => confirm(`Delete "${path}"?`) && merge({
            url: url('delete', path), // method: 'DELETE',
            success: () => {
                console.log(`"${path}" delete success`);
                location.reload();
            },
            error: (x, c, ret) => alert(`Delete "${path}" failed: ${ret}!`)
        }, opt),
        uploadFile: (filename, text, opt) => {
            if (text instanceof FormData) {
                var data = text;
            } else {
                var data = new FormData();
                if (text === String(text)  || text instanceof File) {
                    text = new Blob([text], { type: 'text/plain' });
                } else if (!text instanceof Blob) {
                    return console.error('Invalid file content');
                }
                data.append('data', text, filename);
            }
            merge({ url: url('upload'), method: 'POST', data: data }, opt);
        }
    }
})();

var formatSize = (function() {
    var units = ["B", "K", "M", "G", "T", "P"];
    var decimals = [0, 1, 2, 3, 3, 3];
    return function(size, base=1024) {
        let exp = size ? (Math.log2(size) / Math.log2(base)) : 0;
        exp = Math.min(Math.floor(exp), units.length - 1);
        return (size / (base**exp)).toFixed(decimals[exp]) + units[exp];
    };
})();
