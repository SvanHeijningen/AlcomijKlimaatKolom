using Newtonsoft.Json;
using System;
using System.Collections.Generic;
using System.Collections.Immutable;
using System.Globalization;
using System.Linq;
using System.Threading.Tasks;
using TemperatureEqualizer.ThingsboardDtos;

namespace TemperatureEqualizer
{
    class Equalizer
    {
        public ThingsboardHelper Helper { get; }

        public Equalizer(ThingsboardHelper helper)
        {
            Helper = helper;
        }

        public async Task DoEqualizeAsync()
        {
            IEnumerable<Device> climateControllers = await GetClimateControllers();

            var key = "temp_2";
            // Get live controllers: controllers that have recent telemetry are likely to respond.            
            var latestTelemetry = await GetLatestTelemetry(climateControllers, key);
            IEnumerable<Device> liveDevices = latestTelemetry.Keys;
            await SetAllToMeasurePosition(liveDevices);
            Console.WriteLine("Waiting for stable readings");
            await Task.Delay(TimeSpan.FromMinutes(1));

            latestTelemetry = await GetLatestTelemetry(liveDevices, key);

            if (latestTelemetry.Count > 4)
            {
                var values = latestTelemetry.Values.Select(h => h.Value).ToImmutableSortedSet();

                Console.WriteLine($"Average {key}: {values.Average():0.00}");
                double minimum = values.First();
                Console.WriteLine($"Min {key}: {minimum:0.00}");
                Console.WriteLine($"Max {key}: {values.Last():0.00}");

                double quartile = values[values.Count / 4];
                Console.WriteLine($"First quartile {key}: {quartile:0.00}");
                Console.WriteLine($"Median         {key}: {values[values.Count / 2]:0.00}");
                Console.WriteLine($"Last quartile  {key}: {values[3 * values.Count / 4]:0.00}");

                await Task.WhenAll(latestTelemetry.Select(kv => SetFanAndValveAsync(minimum, kv)));
            }
            else
            {
                Console.WriteLine("Not enough data");
            }
        }

        private async Task SetAllToMeasurePosition(IEnumerable<Device> climateControllers)
        {
            Console.WriteLine("Set all to measurement position");
            foreach (var device in climateControllers)
            {
                try
                {
                    await SetFanAndValveAsync(device, 40, 50);
                }
                catch (Exception e)
                {
                    Console.Error.WriteLine($"Failed {device.Name}: {e.Message}");
                }
            }
        }

        private async Task<Dictionary<Device, Telemetry>> GetLatestTelemetry(IEnumerable<Device> climateControllers, string key)
        {
            var latestTelemetry = new Dictionary<Device, Telemetry>();

            foreach (var device in climateControllers)
            {
                var telemetry = await GetTelemetry(key, device);
                if (telemetry.Timestamp > DateTimeOffset.UtcNow.Subtract(TimeSpan.FromMinutes(1))
                    && !double.IsNaN(telemetry.Value))
                    latestTelemetry.Add(device, telemetry);
            }

            return latestTelemetry;
        }

        private async Task<IEnumerable<Device>> GetClimateControllers()
        {
            var devicesResponse = await Helper.GetResponse($"/api/tenant/devices?limit=50");
            var devices = JsonConvert.DeserializeObject<DataWrapper<Device>>(await devicesResponse.Content.ReadAsStringAsync());

            IEnumerable<Device> ClimateControllers = devices.Data.Where(d => d.Name.StartsWith("KK"));
            return ClimateControllers;
        }

        private async Task<Telemetry> GetTelemetry(string key, Device device)
        {
            Console.Write(device.Name);
            var telemetryResponse = await Helper.GetResponse($"/api/plugins/telemetry/{device.Id.entityType}/{device.Id.Id}/values/timeseries?keys={key}");
            dynamic content = await ThingsboardHelper.DeserializeJson(telemetryResponse);
            var measurement = content[key][0];
            var telemetry = new Telemetry { Ts = measurement.ts, RawValue = measurement.value, Key = key };
            Console.WriteLine($" {telemetry.Timestamp} {telemetry.Key} {telemetry.Value:0.00}");

            return telemetry;
        }

        private async Task SetFanAndValveAsync(double threshold, KeyValuePair<Device, Telemetry> devicevalue)
        {
            // de warmste van boven afzuigen
            int pwm;
            if (devicevalue.Value.Value < threshold)
                pwm = 0;
            else
                // scale fanspeed between 0 (on avg) and 255 (2 deg deviation)
                pwm = (int)(255 * devicevalue.Value.Value - threshold) / 2;

            // set valve to up or down depending on whether it should cool or heat
            var valve = devicevalue.Value.Value > threshold ? 0 /*up*/: 100 /*down*/;

            await SetFanAndValveAsync(devicevalue.Key, pwm, valve);
        }

        private async Task SetFanAndValveAsync(Device key, int fan, int valve)
        {

            if (fan > 255)
                fan = 255;
            if (fan < 0)
                fan = 0;

            if (valve > 100)
                valve = 100;
            if (valve < 0)
                valve = 0;

            Console.WriteLine($"Setting {key.Name} to fan {fan}, valve {valve}");
            var rpcresponse = await Helper.PostRpcAsync(key, "setFanPWM", $"\"{fan}\"");
            var valveresponse = await Helper.PostRpcAsync(key, "setValvePWM", $"\"{valve}\"");
            var message = await rpcresponse.Content.ReadAsStringAsync();
            rpcresponse.EnsureSuccessStatusCode();
            valveresponse.EnsureSuccessStatusCode();
        }
    }
}
