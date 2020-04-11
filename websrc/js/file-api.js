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
    return (method, path, arg) => {
        if (!method in apilist) {
            console.error(`Invalid API name: "${method}"`);
            return "";
        } else
            return eval("`" + apilist[method] + "`");
    }
})();

function ge(id) {
    return document.getElementById(id);
}

function ce(tag) {
    return document.createElement(tag);
}

function downloadFile(path, cb) {
    ajaxXHR({
        url: API("down", path),
        method: "GET",
        success: cb
    });
}

function listDir(path, cb) {
    ajaxXHR({
        url: API("list", path),
        method: "GET",
        dataType: "json",
        success: cb,
    });
}

function createPath(path, isdir, cb) {
    ajaxXHR({
        url: API("create", path, isdir ? "dir" : "file"),
        method: "GET", // or PUT,
        success: cb || function(res, code) {
            console.log(`"${path}" successfully created!`);
            window.location.reload();
        },
        error: (xhr, code, ret)=>alert(`Cannot create "${path}": ${ret}!`),
    });
}

function deletePath(path, cb) {
    if (!confirm(`Delete path "${path}"?`)) return;
    ajaxXHR({
        url: API("delete", path),
        method: "GET", // or DELETE
        success: cb || function(res, code) {
            console.log(`"${path}" successfully deleted!`);
            window.location.reload();
        },
        error: (xhr, code, ret)=>alert(`Cannot delete "${path}": ${ret}!`),
    });
}

var formatSize = (function() {
    var units = ["B", "K", "M", "G", "T", "P"];
    var decimals = [0, 1, 2, 3, 3, 3];
    return function(size, base=1024) {
        let exp = Math.log2(size) / Math.log2(base);
        exp = Math.min(Math.floor(exp), units.length - 1);
        return (size / (base**exp)).toFixed(decimals[exp]) + units[exp];
    };
})();
