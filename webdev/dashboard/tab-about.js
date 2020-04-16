var _tplTabAbout = `
<div class="container">
    <h4 class="mt-4 border-bottom">Dashboard - About</h4>
    <p>Currently v1.0.stopeased at 2020.4.26</p>
    <p>Copyright (c) 2020 Hankso. All rights reserved.</p>
    <h4 class="mt-4 border-bottom">MIT License</h4>
    <p>
        This project is made possible by many open source projects.
    </p>
    <details class="position-relative">
        <summary class="text-success d-inline">See our Licenses</summary>
        <a href="/licenses.txt" target="_blank" class="position-absolute">
            Get full text at this page
            <i class="fas fa-external-link-alt fa-sm"></i>
        </a>
        <div class="border rounded border-success py-2 resize overflow-auto">
            <div class="embed-responsive h-100">
                <iframe src="/licenses.txt" title="License info"></iframe>
            </div>
        </div>
    </details>
    <h4 class="mt-4 border-bottom">Useful links</h4>
    <p>
        If you like this project, please consider donation!
        Welcome <b>star</b> and <b>issue</b> on GitHub.
    </p>
    <section class="links row">
        <a v-for="{title, link, icon} in links" :key="title"
         :href="link" target="_blank" class="col-4 col-lg-auto">
            <i :class="icon"></i>
            {{ title }}
        </a>
    </section>
</div>
`;

var tabAbout = {
    data: () => ({
        links: [
            {
                title: 'Documentation',
                link: '/docs',
                icon: 'fas fa-book'
            },
            {
                title: 'Source code',
                link: 'https://github.com/hankso/cloud3dp',
                icon: 'fab fa-github'
            },
            {
                title: 'Report an issue',
                link: 'https://github.com/hankso/cloud3dp/issues',
                icon: 'fas fa-bug'
            },
            {
                title: 'Star on GitHub',
                link: 'https://github.com/hankso/cloud3dp',
                icon: 'fas fa-star'
            },
            {
                title: 'Contact author',
                link: 'mailto:hankso1106@gmail.com',
                icon: 'fas fa-envelope'
            },
            {
                title: 'Donation',
                link: 'https://www.hankso.site/donate',
                icon: 'fas fa-donate'
            },
        ],
    }),
    mounted: function() {
        let iframes = this.$el.getElementsByTagName('iframe');
        if (iframes.length) iframes[0].onload = function() {
            this.contentDocument.head.innerHTML += `
                <style>
                    pre{
                        font-size: .95em;
                        overflow-x:auto;
                        white-space:pre!important;
                    }
                </style>
            `;
        }
    },
    template: _tplTabAbout
};
