var initVue = () => new Vue({
    data: {
        configs: {}, expands: {}
    },
    computed: {
        namespaces: function() { return Object.keys(this.configs) },
    },
    components: { configEntry },
    methods: {
        subdomains: function(ns) {
            if (ns) return Object.keys(this.configs[ns]);
            return this.namespaces.reduce((arr, ns) => {
                return arr.concat(Object.keys(this.configs[ns]));
            }, []);
        },
        configSave: function() { return configSave(this.configs); },
        configLoad: function() { this.configs = configLoad(); },
        expandSave: function() {
            localStorage.setItem('expands', JSON.stringify(this.expands));
        },
        expandLoad: function() {
            let that = this;
            this.expands = this.namespaces.reduce((obj, ns) => {
                if (!obj.hasOwnProperty(ns)) obj[ns] = {};
                that.subdomains(ns).forEach(sub => {
                    if (!obj[ns].hasOwnProperty(sub)) obj[ns][sub] = false;
                });
                return obj;
            }, JSON.parse(localStorage.getItem('expands') || '{}'));
        },
        expandToggle: function(ns, sub) {
            if (!ns) return;
            let data = this.expands[ns];
            if (sub) data[sub] = !data[sub];
            else {
                let val = !Object.values(data).all();
                Object.keys(data).forEach(key => data[key] = val);
            }
            this.expandSave();
        },
        reload: function() {
            location.reload();
            return false; // preventDefault
        }
    },
});

function onBodyLoad() {
    window.app = initVue();
    ajaxXHR({
        url: "/config",
        dataType: "json",
        success: (cfgs) => {
            configInit(cfgs);
            app.configLoad();
            app.expandLoad();
            app.$mount('#app');
        },
        error: () => document.getElementById('app').innerHTML = `
            Cannot render configuration page.
        `,
    });
}
