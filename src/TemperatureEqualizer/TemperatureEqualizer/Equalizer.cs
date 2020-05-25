using Newtonsoft.Json;
using System;
using System.Collections.Generic;
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
            var devicesResponse = await Helper.GetResponse($"/api/tenant/devices?limit=50");
            var devices = JsonConvert.DeserializeObject<DataWrapper<Device>>(await devicesResponse.Content.ReadAsStringAsync());
            var plantsHumidity = new Dictionary<Device, Telemetry>();
            var key = "temp_2";
            foreach (var device in devices.Data)
            {
                Console.WriteLine(device.Name);
                var telemetryResponse = await Helper.GetResponse($"/api/plugins/telemetry/{device.Id.entityType}/{device.Id.Id}/values/timeseries?keys={key}");
                dynamic content = await ThingsboardHelper.DeserializeJson(telemetryResponse);
                var telemetry = content[key][0];
                if (telemetry.value != null)
                    plantsHumidity.Add(device, new Telemetry { Ts = telemetry.ts, Value = telemetry.value});
            }
            var avg = plantsHumidity.Values.Select(h => h.Value).Average();
            Console.WriteLine($"Average temp_2: {avg}");

           // await Task.WhenAll(plantsHumidity.Select(kv => SetFanAndValveAsync(avg, kv)));
        }

        private async Task SetFanAndValveAsync(double avg, KeyValuePair<Device, Telemetry> devicevalue)
        {
            // scale fanspeed between 0 (on avg) and 255 (2 deg deviation)
            var pwm = (int)(255 * Math.Min(Math.Abs(avg - devicevalue.Value.Value) / 2, 1));

            var fanpwm = $"\"{pwm}\"";
            var rpcresponse = await Helper.PostRpcAsync(devicevalue.Key, "setFanPwm", fanpwm);

            string valvePwm;
            if (devicevalue.Value.Value < avg)
                valvePwm = "0";
            else
                valvePwm = "100";
            var valveresponse = await Helper.PostRpcAsync(devicevalue.Key, "setValvePWM", valvePwm);
            var message = await rpcresponse.Content.ReadAsStringAsync();
            rpcresponse.EnsureSuccessStatusCode();
            valveresponse.EnsureSuccessStatusCode();
        }
    }
}
