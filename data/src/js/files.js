function get_file_list(dir='/') {
    $.ajax({
        url: '/list?dir=' + dir,
        method: 'GET',
        dataType: 'json',
        success: function(files) {
            // for (var i=0; i < files.length; i++) {
            //
            // }
        }
    });
}

(function load_jquery() {
    var jquery = document.createElement('script');
    jquery.src = '/js/jquery-3.4.1.min.js';
    jquery.type = 'text/javascript';
    jquery.onload = function() {
        var $ = window.jQuery;
        var path = $('title').text();
        $('title').text('Index of ' + path);
        $(function() {
            console.log('running after page loaded');
            var files = get_file_list(path);
            // render(files);
            // or to use DataTables (https://datatables.net)
            // $('table').DataTable({
            //     ajax: '/list?dir=' + path,
            //     columns: [
            //         {data: 'Name'},
            //         {data: 'Size'},
            //         {data: 'Data Modified'}
            //     ]
            // });
            // var pathlink = document.createComment('h1');
            // pathlink.innerHTML = 'Index of ';
//            $('body').insertBefore($('<span class="fa fa-upload"></span>'));
        });
    };
    var facss = document.createElement('link');
    facss.rel = 'stylesheet';
    facss.href = '/css/fontawesome.min.css';
    document.getElementsByTagName("head")[0].appendChild(jquery);
    document.getElementsByTagName("head")[0].appendChild(facss);
})();
