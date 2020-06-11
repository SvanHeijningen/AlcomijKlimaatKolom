using Newtonsoft.Json;
using System;
using System.Net;
using System.Net.Http;
using System.Net.Http.Headers;
using System.Text;
using System.Threading.Tasks;
using TemperatureEqualizer.ThingsboardDtos;

namespace TemperatureEqualizer
{
    class ThingsboardHelper
    {
        HttpClient client = new HttpClient();

        public ThingsboardHelper(string baseUri, NetworkCredential credential)
        {
            BaseUri = baseUri;
            Credential = credential;
        }

        public string BaseUri { get; }

        public async Task<HttpResponseMessage> GetResponse(string path)
        {
            var response = await client.GetAsync(BaseUri + path);
            /* string value = await response.Content.ReadAsStringAsync();
             var pretty = JsonConvert.SerializeObject(JsonConvert.DeserializeObject(value), Formatting.Indented);
             Console.WriteLine(pretty);*/
            response.EnsureSuccessStatusCode();

            return response;
        }
        private NetworkCredential Credential { get; }

        public async Task Login()
        {
            Console.WriteLine($"Login with {Credential.UserName}, password length {Credential.Password.Length}");
            var model = new { username = Credential.UserName, password = Credential.Password };
            var content = new StringContent(JsonConvert.SerializeObject(model), Encoding.UTF8, "application/json");
            var response = await client.PostAsync($"{BaseUri}/api/auth/login", content);
            response.EnsureSuccessStatusCode();
            dynamic userToken = await DeserializeJson(response);

            client.DefaultRequestHeaders.Accept.Add(new MediaTypeWithQualityHeaderValue("application/json"));
            client.DefaultRequestHeaders.Add("X-Authorization", "Bearer " + userToken.token);
        }

        public static async Task<dynamic> DeserializeJson(HttpResponseMessage response)
        {
            string data = await response.Content.ReadAsStringAsync();
            dynamic result = JsonConvert.DeserializeObject(data);
            return result;
        }

        public async Task<HttpResponseMessage> PostRpcAsync(Device devicevalue, string method, string param)
        {
            var body = $"{{\"method\":\"{method}\", \"params\":{param}}}";
            var content = new StringContent(body, Encoding.UTF8, "application/json");
            var response = await client.PostAsync($"{BaseUri}/api/plugins/rpc/oneway/{devicevalue.Id.Id}", content);
            return response;
        }


    }
}
