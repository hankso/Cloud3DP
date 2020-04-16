var tabWebcam = {
    // This only works when router-view is wrapped by keep-alive.
    // Or the component will get destroyed when router link changes.
    // The watchers are also deleted, thus callback won't be invoked.
    watch: {
        '$route': function(to, from) {
            // The components seperate from router links.
            // We don't know if current route is for this component.
            this.$router.options.routes.forEach(({path, component}) => {
                if (path != from.path || component !== tabWebcam) return;
                console.log('Leaving from webcam. Auto-stop stream');
                this.stop();
            });
        }
    },
    methods: {
        enter: function() {},
        start: function() {},
        stop: function() {},
        exit: function() {}
    },
    template: `
        <div class="container-fluid">
            TODO: online web camera API
        </div>
    `
};
