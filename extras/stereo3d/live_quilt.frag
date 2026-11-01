#version 330 core
uniform usampler2D Atlas;
uniform ivec2 ViewSize;
uniform ivec2 Grid;
out vec4 Color;
uint raw(int x, int y) { return texelFetch(Atlas, ivec2(x, y), 0).r; }
vec3 palette(uint index, int y)
{
    uint value = raw(1408 + int(index), y);
    return vec3(float((value >> 11u) & 31u) / 31.0,
                float((value >> 5u) & 63u) / 63.0,
                float(value & 31u) / 31.0);
}
uint lookup(int table, uint index)
{
    return raw(int(index & 2047u), 576 + table * 32 + int(index >> 11u));
}
int displacement(float direction, int depth)
{
    float value = direction * float(depth);
    return value < 0.0 ? -int(floor(-value + 0.5)) : int(floor(value + 0.5));
}
void main()
{
    ivec2 p = ivec2(gl_FragCoord.xy);
    p.y = ViewSize.y * Grid.y - 1 - p.y;
    int view = (p.y / ViewSize.y) * Grid.x + p.x / ViewSize.x;
    int y = ViewSize.y - 1 - p.y % ViewSize.y;
    int x = p.x % ViewSize.x - int(raw(1666, y));
    int width = int(raw(1664, y)) - 2 * int(raw(1666, y));
    if (x < 0 || x >= width || (raw(1673, y) != 0u && x < 8))
    {
        Color = vec4(palette(64u, y), 1.0);
        return;
    }
    if (raw(1672, y) != 0u)
    {
        Color = vec4(palette(raw(x + 16, y), y), 1.0);
        return;
    }
    float direction = float(2 * view) / float(Grid.x * Grid.y - 1) - 1.0;
    if (raw(1670, y) != 0u) direction = -direction;
    int aOffset = displacement(direction, int(raw(1667, y)) - 16);
    int bOffset = displacement(direction, int(raw(1668, y)) - 16);
    int sOffset = displacement(direction, int(raw(1669, y)) - 16);
    uint a = raw(x - aOffset + 16, y);
    uint b = raw(352 + x - bOffset + 16, y);
    uint s = raw(704 + x - sOffset + 16, y);
    uint window = raw(1056 + x + 16, y);
    if ((window & 128u) != 0u) a = window & 127u;
    bool sh = raw(1671, y) != 0u;
    uint background = lookup(sh ? 2 : 0, (b << 8u) | a);
    uint result = lookup(sh ? 3 : 1, (background << 8u) | s);
    Color = vec4(palette(result, y), 1.0);
}
