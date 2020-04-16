var menuBox = {
    inject: ['treeRender', 'treeRoot', 'statusLog'],
    data: () => ({
        menuTab: '', uploadPath: '/', uploadType: 'file'
    }),
    created: function() { this.initValues() },
    methods: {
        initValues: function() {
            this.uploadPath = this.treeRoot;
            this.menuTab = ''; this.files = [];
        },
        menuToggle: function(target) {
            if (!this.menuTab) this.uploadPath = this.treeRoot;
            this.menuTab = this.menuTab == target ? '' : target;
        },
        updateName: function(e) {
            // copy filename to uploadPath for further modification
            if (e) this.files = e.target.files;
            if (!this.files.length) return;
            this.uploadPath = this.treeRoot + this.files[0].name;
        },
        createPath: function() {
            if (!this.uploadPath || !this.uploadType) {
                return this.statusLog('Invalid path name/type');
            } else if (!this.uploadPath.startsWith('/')) {
                this.uploadPath = this.treeRoot + this.uploadPath;
            }
            let that = this, path = this.uploadPath, type = this.uploadType;
            API.createPath(path, type != 'file', {
                success: () => {
                    that.statusLog(`${type} "${path}" created success`);
                    setTimeout(that.treeRender, 500);
                    that.initValues();
                },
                error: (xhr, code, text) => that.statusLog(
                    `Create ${type} "${path}" failed[${code}]: ${text}`
                ),
                progress: this.statusProgress
            });
        },
        uploadFile: function() {
            if (!this.files.length) {
                return this.statusLog('No file selected to upload');
            } else if (this.uploadPath.endsWith('/')) {
                return this.statusLog(`Invalid path: "${this.uploadPath}"`);
            } else if (!this.uploadPath.startsWith('/')) {
                this.uploadPath = this.treeRoot + this.uploadPath;
            }
            let that = this, path = this.uploadPath;
            API.uploadFile(path, this.files[0], {
                success: () => {
                    that.statusLog(`"${path}" upload success`);
                    setTimeout(that.treeRender, 500);
                    that.initValues();
                },
                error: (xhr, code, text) => that.statusLog(
                    `Upload "${path}" failed[${code}]: ${text}`
                ),
                progress: this.statusProgress
            });
        }
    },
    template: `
        <div class="menu-box">
            <slot></slot>
            <a @click="menuToggle('upload')" title="upload file">
                <i class="far fa-arrow-alt-circle-up info"></i>
            </a>
            <a @click="menuToggle('create')" title="create path">
                <i class="far fa-plus-square info"></i>
            </a>
            <form :class="menuTab" v-if="menuTab" @keypress.enter.prevent>
                <div>
                    <label>Path name</label>
                    <input type="text" v-model="uploadPath">
                </div>
                <transition name="fade" mode="out-in">
                    <div v-if="menuTab == 'upload'" :key="menuTab">
                        <input type="file" @change="updateName">
                        <button @click.prevent="uploadFile">Upload</button>
                    </div>
                    <div v-if="menuTab == 'create'" :key="menuTab">
                        <label>Path type</label>
                        <select v-model="uploadType">
                            <option disabled value="">Select type</option>
                            <option value="file">file</option>
                            <option value="folder">folder</option>
                        </select>
                        <button @click.prevent="createPath">Create</button>
                    </div>
                </transition>
            </form>
        </div>
    `
};

var statusMixin = {
    components: { menuBox },
    data: () => ({
        status: {
            data: '', handle: 0, rate: '',
        }
    }),
    methods: {
        statusProgress: function(e) {
            if (e.lengthComputable) {
                this.status.rate = (e.loaded / e.total * 100).toFixed();
            } else {
                this.statusLog(`Received ${formatSize(e.loaded)}`);
            }
            if (e.loaded == e.total) {
                let that = this;
                setTimeout(() => {
                    that.status.rate = '' ;
                }, 500);
            }
        },
        statusLog: function(msg) {
            let that = this;
            console.log(msg);
            this.status.data = `${new Date().toLocaleTimeString()} - ${msg}`;
            clearTimeout(this.status.handle);
            this.status.handle = setTimeout(() => {
                that.status.data = '';
            }, 5000);
        }
    },
    provide: function() {
        return {
            statusProgress: this.statusProgress,
            statusLog: this.statusLog
        }
    }
};
