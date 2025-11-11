using System;
public class Player
{
    private int entityId;

    public void Init()
    {
        //entityId = engine.CreateEntity("Player");
        Console.WriteLine("Init: Player script initialized!");
    }

    public void Update(float dt)
    {
        // simple animation
        float x = (float)(Math.Sin(DateTime.Now.TimeOfDay.TotalSeconds) * 2.0);
        Console.WriteLine($"Update: Player position x: {x}");
        //engine.SetEntityPosition(entityId, x, 0, 0);
    }
}
