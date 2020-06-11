using System;
using System.Globalization;

namespace TemperatureEqualizer.ThingsboardDtos
{
    class Telemetry
    {
        public long Ts;

        public string RawValue;
        public double Value
        {
            get
            {
                return double.TryParse(RawValue, NumberStyles.Any, CultureInfo.InvariantCulture, out var result) ? result : double.NaN;
            }
        }

        public DateTimeOffset Timestamp => DateTimeOffset.FromUnixTimeMilliseconds(Ts);

        public string Key { get; internal set; }
    }

}
