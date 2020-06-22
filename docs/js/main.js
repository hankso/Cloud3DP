window.$docsify = {
    // General
    name: 'Cloud3DP',
    repo: 'hankso/Cloud3DP',
    loadNavbar: 'nav.md',
    loadSidebar: 'sidebar.md',
    coverpage: 'cover.md',
    homepage: 'README.md',
    
    // Render
    autoHeader: false,
    mergeNavbar: true,
    topMargin: 50,
    themeColor: '#b4a078',
    formatUpdated: '{YY}/{MM}/{DD} {HH}:{mm}',

    // Navigation
    alias: {
        '/.*/sidebar.md': '/sidebar.md',
        '/.*/nav.md': '/nav.md',
    },
    auto2top: true,
    maxLevel: 3,
    subMaxLevel: 3,

    // Plugins
    search: {
        depth: 3,
        placeholder: 'Search...',
        maxAge: 86400000,
    },
    plugins: [
        function(hook, vm) {
            let repo = $docsify.repo;
            let base = 'https://github.com/' + repo + '/edit/master/docs/';
            hook.beforeEach(md => [
                '[Edit on GitHub](' + base + vm.route.file + ')', md
            ].join('\n\n'));
            hook.afterEach((html, next) => next([
                html, '<hr>',
                '<footer>',
                'Copyright &copy; 2020-present ',
                '<a target="_blank" href="https://github.com/hankso">Hank</a>.',
                'Powered by',
                '<a target="_blank" href="https://docsify.js.org">docsify</a>.',
                '</footer>'
            ].join('\n')));
        }
    ]
};

if (typeof navigator.serviceWorker !== 'undefined') {
    navigator.serviceWorker.register('js/sw.js')
}
