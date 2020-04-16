var sortArrow = {
    props: ['inc', 'index', 'target'],
    model: { prop: 'target', event: 'assign' },
    template: `
        <span @click="$emit('assign', index); $emit('change');" class="sort">
            <slot></slot>
            <i :class="['fas', 'fa-arrow-' + (inc ? 'up' : 'down')]"></i>
        </span>
    `
};

var fileInfo = {
    props: ['file'],
    created: function() {
        // aliases
        this.name = this.file.name;
        this.type = this.file.type;
        this.size = this.isdir ? '' : formatSize(this.file.size);
        this.date = new Date(this.file.date).toLocaleString();
    },
    methods: {
        remove: function() { API.deletePath(this.name); }
    },
    computed: {
        isdir: function() { return this.type != 'file' },
        gzip: function() { return this.name.endsWith('.gz') },
        link: function() { return window.root + this.name },
        edit: function() { return API.url('edit', this.name) },
        down: function() { return API.url('down', this.name) },
    },
    template: `
        <div class="file" :type="type">
            <a :href="link">
                <i :class="['far', 'fa-fw', 'fa-' + type]"></i>
                {{ name + (isdir ? '/' : '') }}
            </a>
            <span class="details size">{{ size }}</span>
            <span class="details date">{{ date }}</span>
            <span class="actions">
                <a @click="remove" :title="'delete ' + name">
                    <i class="far fa-trash-alt"></i>
                </a>
                <a class="far fa-edit" v-if="!isdir && !gzip"
                 :href="edit" :title="'online edit file ' + name"></a>
                <a class="far fa-arrow-alt-circle-down" v-if="!isdir"
                 :href="down" :download="name" target="_blank"
                 :title="'download file ' + name"></a>
            </span>
        </div>
    `
};

var initVue = () => new Vue({
    components: { sortArrow, fileInfo },
    data: {
        fileList: [], root: '/',
        orderList: [-1, 1, 1], // increment or decrement
        sortIdx: 0, // pass arguments to method sort
        sortKeys: ['name', 'size', 'date'],
        sortNames: ['Name', 'Size', 'Last Modified'],
    },
    computed: {
        files: function() {
            return this.fileList.filter(({ name }) => (
                !['.', '..'].includes(name)) &&
                !(window.hide && name.startsWith('.')) &&
                !(window.nogzip && name.endsWith('.gz'))
            );
        },
        parents: function() {
            let len = this.root.length - 1,
                parents = this.root.substr(0, len).split('/');
            return parents.map((parent, idx) => ({
                path: ` ${parent} /`,
                link: parents.slice(0, idx + 1).join('/') + '/'
            }));
        }
    },
    methods: {
        sort: function() {
            // change order on each click
            let oldOrder = this.orderList[this.sortIdx],
                newOrder = 0 - oldOrder,
                keyName = this.sortKeys[this.sortIdx];
            this.orderList[this.sortIdx] = newOrder;
            //localStorage.setItem('orderList', this.orderList);
            this.fileList.sort((f1, f2) => (
                f1[keyName] > f2[keyName] ? newOrder : (
                    f1[keyName] < f2[keyName] ? oldOrder : 0
                )
            ));
        }
    }
});

function onBodyLoad() {
    // parse parameters from URL
    let search = new URLSearchParams(window.location.search);
    window.hide = search.get('hide') != 'false';
    window.nogzip = search.get('nogzip') != 'false';

    if (root.indexOf('%') >= 0) root = window.location.pathname;
    if (search.has('dir')) root = search.get('dir');
    if (!root.endsWith('/')) {
        root = root.endsWith('html') ? '/' : (root + '/');
    }
    let title = document.getElementsByTagName('title')[0];
    title.innerText = `Index of ${root} | ${title.innerText}`;

    try {
        window.app = initVue();
        start(fileList = JSON.parse(fileList));
    } catch(e) {
        if (e instanceof SyntaxError /* JSON parse error */) {
            // fetch with ajaxXHR in file-api.js
            API.listDir(root, {
                success: json => start(fileList = json),
                error: () => error(`
                    Neither files information nor 'js/file-api.js' 
                    is provided. Cannot render files tree under 
                    "${root}". Abort!
                `)
            });
        } else error(e.toString());
    }
}

function error(msg) {
    ge('header').innerHTML = 'Error';
    ge('thead').innerHTML = '';
    ge('tbody').innerHTML = msg;
}

function start(json) {
    let deepcopy = JSON.stringify(json);
    app.fileList = JSON.parse(deepcopy);
    app.root = window.root;
    app.$mount('#app');
    ge('data').remove();
}
