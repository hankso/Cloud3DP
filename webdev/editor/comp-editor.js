function createEditor(e, theme) {
    ace.Editor.prototype.setValueClear = function(text) {
        this.setValue(text);
        this.clearSelection();
    }
    let editor = ace.edit(e, {
        theme: 'ace/theme/' + (theme || 'textmate'),
        //autoScrollEditorIntoView: true,
        highlightActiveLine: true,
        showFoldWidgets: true,
        showPrintMargin: false,
    });
    ace.require('ace/ext/settings_menu').init(editor);
    ace.require('ace/ext/keybinding_menu').init(editor);
    // Editor configuration
    editor.session.setUseSoftTabs(true);
    editor.session.setTabSize(4);
    editor.setHighlightActiveLine(true);
    editor.setShowFoldWidgets(true);
    editor.setShowPrintMargin(false);
    editor.commands.addCommands([
        {
            name: 'settings',
            bindKey: {win: 'Ctrl-Q', mac: 'Command-Q'},
            exec: editor => editor.showSettingsMenu(),
            readOnly: false
        },
        {
            name: 'keybings',
            bindKey: {win: 'Ctrl-H', mac: 'Command-H'},
            exec: editor =>editor.showKeyboardShortcuts(),
        }
    ]);
    return editor;
}

var editorComponent = {
    props: ['name', 'filename'],
    model: { prop: 'filename', event: 'set-filename' },
    inject: ['statusLog', 'statusProgress', 'config'],
    data: () => ({
        modified: false, editor: null, lang: 'plain', semaphore: false,
    }),
    template: `
        <div :id="name" class="custom-sb">
            <div v-if="!isimg" class="editor" :id="element"></div>
            <div v-if="isimg" class="preview">
                <img src="link">
            </div>
        </div>
    `,
    watch: {
        filename: function() { this.load() }
    },
    computed: {
        link: function() { return API.url('edit', this.filename) },
        valid: function() { return !['', '/', '.'].includes(this.filename) },
        element: function() { return this.name + '-text' },
        extname: function() {
            let names = this.filename.split('.');
            return names.length < 2 ? '' : names[1];
        },
        isimg: function() {
            return ['png', 'jpg', 'gif', 'ico', 'bmp'].includes(this.extname)
        },
        filelang: function() {
            var extname = this.extname, lang;
            switch(extname) {
                case 'ts': case 'js': lang = 'javascript'; break;
                case 'txt': case 'hex': lang = 'plain'; break;
                case 'scss': case 'css': lang = 'css'; break;
                case 'conf': case 'ini': lang = 'ini'; break;
                case 'html': case 'htm': lang = 'html'; break;
                case 'md': case 'markdown': lang = 'md'; break;
                case 'h': case 'c': case 'cpp': lang = 'c_cpp'; break;
                case 'gcode': case 'json': case 'xml': lang = extname; break;
                default: lang = this.config.lang;
            }
            return lang;
        }
    },
    mounted: function() {
        let that = this;
        this.editor = createEditor(this.element, this.config.theme);
        this.editor.on('change', () => that.mofified = true);
        this.editor.commands.addCommand({
            name: 'save',
            bindKey: {win: 'Ctrl-S',  mac: 'Command-S'},
            exec: this.save,
            readOnly: false
        });
        this.load();
    },
    methods: {
        load: function() {
            if (!this.valid) {
                this.editor.setValueClear('Select one file to edit...');
                return this.statusLog('No file to load');
            }
            if (this.modified) {
                this.statusLog('log', 'auto-saving...');
                this.editor.execCommand('save');
            }
            this.modified = false;
            if (this.isimg) return;
            // set language highlight
            if (this.lang != this.filelang) {
                this.lang = this.filelang;
                this.editor.session.setMode('ace/mode/' + this.lang);
            }
            // fetch file content
            let that = this, path = this.filename;
            ajaxXHR({
                url: this.link, method: "GET", success: ret => {
                    that.editor.setValueClear(ret);
                    that.statusLog(`Editing file "${path}"`);
                    that.modified = false;
                }, error: (xhr, code, text) => {
                    that.editor.setValueClear(
                        `Load "${path}" failed: ${code} - ${text}`);
                    that.$emit('set-filename', '/');
                },
                progress: this.statusProgress
            });
        },
        save: function() {
            if (!this.valid) return this.statusLog('No opened file');
            if (this.semaphore)
                return this.statusLog('Busy saving file');
            else
                this.semaphore = true;
            let that = this, path = this.filename;
            API.uploadFile(path, this.editor.getValue() + '', {
                success: () => {
                    that.statusLog(`"${path}" saved success`);
                    that.modified = that.semaphore = false;
                },
                error: (xhr, code, text) => {
                    that.statusLog(`Save ${path} failed: ${code} - ${text}`);
                    that.semaphore = false;
                },
                progress: this.statusProgress
            });
        }
    },
};

var editorMixin = {
    components: { 'editor': editorComponent },
    methods: {
        dirname: (fn, sep='/') => fn.substr(0, fn.lastIndexOf(sep) + 1),
        basename: (fn, sep='/') => fn.substr(fn.lastIndexOf(sep) + 1),
        join: function(args, sep='/') {
            return args.join(sep).replace(new RegExp(sep + '+', 'g'), sep);
        },
        // helper function to switch between multiple files
        loadFile: function(filename) {
            if (typeof filename != 'string') return console.error(filename);
            if (!filename || filename == this.config.filename) return;
            // resolve pathname of new opening file
            // filename may be like '[/][path/to/]{file|folder}[/]'
            let [i, head, j, end] = /^(\/)?(.*\w+)?(\/)?$/g.exec(filename);
            let dirname = this.dirname(this.config.filename);
            if (end) {
                // opening directory: auto redirect to 'index.html'
                if (head) dirname = filename;
                else dirname = this.join([dirname, filename]);
                filename = this.join([dirname, 'index.html']);
            } else if (head) {
                // open full path file
                dirname = this.dirname(filename);
            } else {
                // open file under current directory
                filename = this.join([dirname, filename]);
            }
            this.config.filename = filename;
            if (dirname != this.treeRoot) {
                this.treeSwitch(dirname);
            }
        }
    },
    provide: function() {
        return { loadFile: this.loadFile };
    }
};
