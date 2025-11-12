public class PlayerController : Script
{
    Transform transform;
    Velocity velocity;

    public override void OnInit()
    {
        transform = GetComponent<Transform>();
        velocity = GetComponent<Velocity>();
    }

    public override void OnUpdate(float dt)
    {
        var pos = transform.Position;
        var vel = velocity.Value;
        pos.X += vel.X * dt;
        pos.Y += vel.Y * dt;
        transform.Position = pos;
    }
} 
