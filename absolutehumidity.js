
function getAbsoluteHumidity(temperature, humidity) {
    return (6.112 * Math.exp((17.67 * temperature) / (
            temperature + 243.5)) * humidity * 2.1674) /
        (273.15 + temperature);
}

function getDewPoint(temperature, humidity) {
    return (243.5 * (Math.log(humidity / 100) + ((17.67 * temperature) / (243.5 + temperature)))) / 
 (17.67 - Math.log(humidity / 100) - ((17.67 * temperature) / (243.5 + temperature))); 

}


if (msg["temp_1"] > 0 && msg["hum_1"] > 0) {
    /*All math is accurate within 1% between 0 and 35 degrees Celcius*/
    /* 
     * https://carnotcycle.wordpress.com/2012/08/04/how-to-convert-relative-humidity-to-absolute-humidity/ 
     */
    /// <summary> 
    /// in gr/m³ 
    /// </summary> 

    msg["abs_1"] = parseFloat(getAbsoluteHumidity(msg["temp_1"], msg["hum_1"]).toFixed(2));
    msg["dew_1"] = parseFloat(getDewPoint(msg["temp_1"], msg["hum_1"]).toFixed(2));
}
return {
    msg: msg,
    metadata: metadata,
    msgType: msgType
};

