<!DOCTYPE html>
<html>
<head>
    <title>Temp Control</title>
    <meta http-equiv="content-type" content="text/html; charset=utf-8">
    <script src="/assets/js/jquery.min.js"></script>
    <script src="/assets/js/echarts.min.js"></script>
    <script src="js/echart-option.js"></script>
</head>

<body>
    <h2>Temprature Display Page</h2>
    <span class="fa fa-play" id="toggle"></span>
    
    <div id='echart' style="width:90%; min-height:400px; margin:50px 0;"></div>

    <script type="text/javascript">
        var chart;
        $(function() {
            chart = echarts.init(document.getElementById("echart"));
            chart.setOption(option);
            window.addEventListener("resize", chart.resize);
            function loopTask() {
                $.ajax({
                    url: "/temp",
                    dataType: "json",
                    method: "GET",
                    success: function(msg) {
                        var date = Date.now();
                        chart.setOption({
                            xAxis: {
                                max: date
                            }
                        });
                        for (var i = 0; i < msg.length; i++) {
                            chart.appendData({
                                seriesIndex: i,
                                data: [
                                    [date, msg[i]]
                                ]
                            });
                        }
                    }
                });
            }
            $('#toggle').click(function(){
                if ($(this).hasClass('fa-pause')) {
                    clearInterval(t);
                } else if ($(this).hasClass('fa-play')) {
                    t = setInterval(loopTask, 1000);
                }
                $(this).toggleClass('fa-play');
                $(this).toggleClass('fa-pause');
            })
        });
    </script>
</body>
</html>
