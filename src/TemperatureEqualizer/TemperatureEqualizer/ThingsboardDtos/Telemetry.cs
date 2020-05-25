using System;

namespace TemperatureEqualizer.ThingsboardDtos
{
    class Telemetry
    {
        public long Ts;
        public double Value;
        public DateTimeOffset Timestamp => DateTimeOffset.FromUnixTimeMilliseconds(Ts);

        public string Key { get; internal set; }
    }

}
