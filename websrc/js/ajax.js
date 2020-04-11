if (typeof XMLHttpRequest === "undefined") {
    XMLHttpRequest = function () {
        try { return new ActiveXObject("Msxml2.XMLHTTP.6.0"); } catch (e) {}
        try { return new ActiveXObject("Msxml2.XMLHTTP.3.0"); } catch (e) {}
        try { return new ActiveXObject("Microsoft.XMLHTTP"); } catch (e) {}
        throw new Error("This browser does not support XMLHttpRequest.");
    };
}

// ajaxXHR("/", (ret)=>console.log(ret))
// ajaxXHR("/", {success: (ret)=>{}, data: {}})
// ajaxXHR({url: "/", success: (ret)=>{}, data: {}})
function ajaxXHR(url, success, config={}) {
    if (url instanceof Object) {
        config = url;
        url = config.url;
    }
    if (success instanceof Function) {
        config.success = config.success || success;
    } else if (success instanceof Object) {
        config = success;
    }
    let xhr = new XMLHttpRequest();
    xhr.withCredentials = Boolean(config.crossorigin) || false;
    xhr.onerror = ()=>console.error("Network error");
    xhr.onprogress = config.progress || function(event) {
        if (event.lengthComputable)
            console.log(`Received ${event.loaded} of ${event.total} bytes`);
        else
            console.log(`Received ${formatSize(event.loaded)}`);
    };
    xhr.onload = function() {
        if (xhr.status != 200) {
            if (config.error) {
                config.error(xhr, xhr.status, xhr.statusText);
            } else {
                console.error(`Error ${xhr.status}: ${xhr.statusText}`);
            }
        } else {
            (config.success || console.log)(xhr.response, xhr.status, xhr);
        }
    };
    xhr.responseType = config.dataType || "";
    xhr.open(config.method || "GET", url, config.async || true);
    if (config.data) {
        var data;
        if (config.data instanceof FormData) {
            data = config.data;
        } else if (config.data instanceof Object) {
            data = new FormData();
            for (let key in config.data) data.append(key, config.data[key]);
        } else if (config.data instanceof String) {
            xhr.setRequestHeader("Content-Type", "application/json");
            data = config.data;
        } else { throw Error("Invalid XMLHttpRequest parameter", config.data); }
        xhr.send(data);
    } else { xhr.send(); }
}
