namespace TemperatureEqualizer.ThingsboardDtos
{
    class Device
    {
        public class EntityId { public string entityType; public string Id; }
        public EntityId Id { get; set; }
        public string Type { get; set; }

        public string Name { get; set; }
    }

}
