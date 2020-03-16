var option = {
    title: {
        show: true,
        text: 'Temprature Display Chart',
        link: 'https://echarts.baidu.com',
        subtext: 'realtime plotting',
        subtextStyle: {
            fontWeight: 800,
        },
        padding: [10, 25],
        borderWidth: 1,
        borderRadius: 10
    },
    legend: {
        type: "plain",
        top: 65,
        orient: "horizontal",
        backgroundColor: "rgba(128,128,128,0.1)",
        borderColor: "#E3E3E3",
        borderWidth: 2
    },
    grid: {
        top: 95,
        bottom: 50,
        show: !0
    },
    xAxis: {
        type: "time",
        name: "Time/s",
    },
    yAxis: {
        type: "value",
        name: "Temprature/°C",
        nameLocation: 'middle'
    },
    dataZoom: [{
        type: "slider",
        xAxisIndex: [0],
        handleIcon: "M10.7,11.9v-1.3H9.3v1.3c-4.9,0.3-8.8,4.4-8.8,9.4c0,5,3.9,9.1,8.8,9.4v1.3h1.3v-1.3c4.9-0.3,8.8-4.4,8.8-9.4C19.5,16.3,15.6,12.2,10.7,11.9zM13.3,24.4H6.7V23h6.6V24.4zM13.3,19.6H6.7v-1.4h6.6V19.6z",
        handleSize: "80%",
        handleStyle: {
            color: "#fff",
            shadowBlur: 3
        },
        top: "bottom"
    }, {
        type: "inside",
        yAxisIndex: [0]
    }],
    tooltip: {
        triger: "axis",
        axisPointer: {
            show: !0,
            type: "cross",
            snap: !0
        }
    },
    toolbox: {
        feature: {
            dataZoom: {},
            dataView: {},
            restore: {},
            saveAsImage: {}
        },
        right: 40
    },
    series: [],
    animationDurationUpdate: 1
};

for (var i = 1; i <= 6; i++) {
    option.series.push({
        name: "Temp" + i,
        type: "line",
        smooth: !0,
        data: []
    });
}

function clearSeries(channel = null) {
    for (vari = 0; i < 6; i++) {
        if (channel && channel != i) continue;
        option.series[i].data = [];
    }
}
