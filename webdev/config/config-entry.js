var configEntry = {
    props: ['name', 'value'],
    computed: {
        type: function() {
            let type = typeof(this.value);
            if (type == 'object' && Array.isArray(this.value)) {
                type = 'array';
            }
            return type;
        },
        val: { // mapping v-model on component to v-model inside component
            get: function() { return this.value },
            set: function(val) { this.$emit('input', val) }
        }
    },
    template: `
        <div class="entry" :id="name">
            <label>{{ name.split('-')[3] }}</label>
            <input v-if="type == 'string'" type="text"
             v-model.trim.lazy="val">
            <input v-else-if="type == 'number'" type="number" step="1"
             v-model.number="val">
            <label v-else-if="type == 'boolean'"
             class="switch-box" :for="'cb-' + name">
                <input :id="'cb-' + name" type="checkbox" v-model="val">
                <span class="switch-dot"></span>
            </label>
            <span v-else>Invalid value type({{ type }}): {{ value }}</span>
        </div>
    `,
};

Array.prototype.all = function() {
    return this.reduce((v, e) => v && Boolean(e), true)
}
Array.prototype.any = function() {
    return this.reduce((v, e) => v || Boolean(e), false);
}
