// Priority 0: default values
window.config = {
    filename: '/index.html', // full path to filename (for editor)
    pathname: '/', // full path to current firectory (for tree view)
    lang: 'plain', // hightlight language
    tree: true, // display tree view
    hide: false, // NOT display hidden files ('.', '..', '.*')
    nogzip: false, // NOT display gzip files ('*.gz')
};

var configMixin = {
    data: () => ({ config: window.config }),
    watch: {
        config: {
            handler: function() { this.configSave() },
            deep: true
        }
    },
    methods: {
        configSave: function() {
            let json = JSON.stringify(this.config);
            localStorage.setItem('config', json);
        },
        configLoad: function() {
            // Recover from where you left last time
            let config = JSON.parse(localStorage.getItem('config'));
            this.config = Object.assign(this.config, config);
        }
    },
    provide: function() {
        return { config: this.config }
    }
};

function onBodyLoad() {
    window.app = new Vue({
        mixins: [editorMixin, treeMixin, statusMixin, configMixin]
    });
    // Priority 1: last time values
    app.configLoad();
    // Priority 2: URL parameters
    let search = new URLSearchParams(window.location.search);
    for (let [k, v] of search) { if (v) app.config[k] = v; }
    app.config.filename = checkRoot(app.config.filename);
    app.config.pathname = checkRoot(app.config.pathname);
    app.config.hide = checkFalse(app.config.hide);
    app.config.nogzip = checkFalse(app.config.nogzip);
    app.$mount('#app');
}

function checkFalse(v) {
    return v == 'false' ? false : Boolean(v);
}

function checkRoot(v) {
    return v.startsWith('/') ? v : '/' + v;
}
