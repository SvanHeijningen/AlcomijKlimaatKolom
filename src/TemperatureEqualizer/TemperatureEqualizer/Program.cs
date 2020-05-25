using System;
using System.Threading.Tasks;

namespace TemperatureEqualizer
{

    class Program
    {
        static async Task Main(string username = "username", string password="***", string baseUri = "http://64.227.77.146:8080")
        { 
            var thingsboard = new ThingsboardHelper(baseUri, new System.Net.NetworkCredential(username, password));
            await thingsboard.Login();
            var equalizer = new Equalizer(thingsboard);
            await equalizer.DoEqualizeAsync();

        }
    }
}
