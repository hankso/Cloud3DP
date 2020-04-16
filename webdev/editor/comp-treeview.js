var leafInfo = {
    props: ['file', 'root'],
    created: function() {
        this.name = this.file.name;
        this.type = this.file.type;
        this.path = this.root + this.name;
        if (this.isdir) this.path += '/';
    },
    computed: {
        isdir: function() { return this.type != 'file' },
        gzip: function() { return this.name.endsWith('.gz') },
        down: function() { return API.url('down', this.name) },
    },
    template: `
        <div class="leaf" :name="name" :disbaled="gzip">
            <a :class="['path', type]" @click="$emit(type, path)"
             :title="isdir ? 'expand directory' : 'edit this file'">
                {{ name + (isdir ? '/' : '') }}
            </a>
            <span class="actions">
                <a :href="path" v-if="isdir" class="fas fa-sitemap fa-sm"
                 target="_blank" :title="'view '+name+' in FileManager'"></a>
                <a :href="down" v-else class="far fa-arrow-alt-circle-down"
                 :download="name" :title="'download file ' + name"></a>
            </span>
        </div>
    `
};

var treeMixin = {
    components: { leafInfo },
    data: { fileList: [] },
    computed: {
        // hide files tree when the editor page is imported as iframe
        treeShow: {
            get: function() { return this.config.tree != 'hide' },
            set: function(val) { this.config.tree = val ? 'show' : 'hide' }
        },
        treeRoot: {
            get: function() { return this.config.pathname },
            set: function(dir) {
                if (!dir.startsWith('/')) dir = this.treeRoot + dir;
                if (!dir.endsWith('/')) dir += '/';
                this.config.pathname = dir;
            }
        },
        treeFile: function() {
            return this.fileList.filter(({ name }) => (
                !['.', '..'].includes(name)) &&
                !(this.config.hide && name.startsWith('.')) &&
                !(this.config.nogzip && name.endsWith('.gz'))
            );
        },
        parents: function() {
            let len = this.treeRoot.length - 1,
                parents = this.treeRoot.substr(0, len).split('/');
            return parents.map((parent, idx) => ({
                path: parent + '/',
                link: parents.slice(0, idx + 1).join('/') + '/'
            }));
        },
    },
    mounted: function() {
        this.treeRender();
    },
    methods: {
        treeToggle: function() {
            let newVal = (this.treeShow = !this.treeShow);
            this.statusLog('Tree view ' + (newVal ? 'enabled' : 'disabled'));
        },
        treeEntend: function() { return 'TODO'; },
        treeSwitch: function(dir) {
            if (typeof dir !== 'string') return;
            this.treeRoot = dir;
            this.treeRender();
        },
        treeRender: function() {
            let that = this;
            API.listDir(this.treeRoot, {
                success: fileList => {
                    if (that.config.sort) fileList.sort((a, b) => (
                        a.name < b.name ? -1 : ((a.name > b.name) ? 1 : 0)
                    ));
                    window.fileList = fileList,
                    that.fileList = JSON.parse(JSON.stringify(fileList));
                    that.statusLog("Tree view refreshed");
                },
                error: (e, c, ret) => {
                    that.statusLog('Tree view render failed: ' + ret);
                }
            });
        },
    },
    provide: function() {
        // expose these data/methods to sub components
        return {
            treeRender: this.treeRender,
            treeRoot: this.treeRoot,
        };
    }
};
