#version 440

layout(std140, binding = 0) uniform buf {
    mat4 uClip;
    vec4 p0; // width, height, dpr, barCount
    vec4 p1; // barWidth, spacing, centered, rounded
    vec4 p2; // circleMode, circleSize, style, waveMode
    vec4 p3; // fillWave, drawInactive, blockHeight, blockSpacing
    vec4 p4; // gradientOrientation, waveform, waveSimulateWaveform, circleModeFill
    vec4 p5; // waveFillOrientation, inactiveOrientation, unused, unused
};

layout(binding = 1) uniform sampler2D uSpectrum;
layout(binding = 2) uniform sampler2D uActiveLut;
layout(binding = 3) uniform sampler2D uWaveFillLut;
layout(binding = 4) uniform sampler2D uInactiveLut;

layout(location = 0) in vec2 v_uv;
layout(location = 0) out vec4 fragColor;

float valueAt(int i)
{
    float count = max(p0.w, 1.0);
    return texture(uSpectrum, vec2((float(i) + 0.5) / count, 0.5)).r;
}

float distanceToSegment(vec2 p, vec2 a, vec2 b)
{
    vec2 ab = b - a;
    float denominator = dot(ab, ab);
    float t = denominator > 1e-6 ? clamp(dot(p - a, ab) / denominator, 0.0, 1.0) : 0.0;
    return length(p - (a + t * ab));
}

float edgeWidth()
{
    return 0.5 / max(p0.z, 1.0);
}

float coverage(float signedDistance, float aa)
{
    return 1.0 - smoothstep(-aa, aa, signedDistance);
}

float boxDistance(vec2 p, vec2 center, vec2 halfSize)
{
    vec2 q = abs(p - center) - halfSize;
    return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0);
}

float gradientCoordinate(vec2 px, float orientation)
{
    if (orientation > 0.5) {
        return 1.0 - px.y / max(p0.y, 1.0);
    }
    return px.x / max(p0.x, 1.0);
}

vec4 sampleLut(sampler2D lut, vec2 px, float orientation)
{
    float t = clamp(gradientCoordinate(px, orientation), 0.0, 1.0);
    vec4 color = texture(lut, vec2(t, 0.5));
    return vec4(color.rgb * color.a, color.a);
}

vec4 activeColor(vec2 px)
{
    return sampleLut(uActiveLut, px, p4.x);
}

vec4 waveFillColor(vec2 px)
{
    return sampleLut(uWaveFillLut, px, p5.x);
}

vec4 inactiveColor(vec2 px)
{
    return sampleLut(uInactiveLut, px, p5.y);
}

vec4 renderBars(vec2 px)
{
    float barWidth = max(p1.x, 1.0);
    float spacing = max(p1.y, 0.0);
    float cell = barWidth + spacing;
    if (p0.w < 1.0) {
        discard;
    }
    int count = int(p0.w);
    int index = int(clamp(floor(px.x / cell), 0.0, float(count - 1)));

    float height = max(p0.y, 1.0);
    float value = clamp(valueAt(index), 1.0, height);
    float normalized = value / height;
    bool centered = p1.z > 0.5;
    bool rounded = p1.w > 0.5;
    float radius = barWidth * 0.5;
    float yBottom;
    float yTop;

    if (centered) {
        float barHeight = rounded ? normalized * ((height - barWidth) * 0.5)
                                  : normalized * (height * 0.5);
        yBottom = height * 0.5 - barHeight;
        yTop = yBottom + barHeight * 2.0;
    } else if (rounded) {
        float barHeight = normalized * (height - barWidth);
        yBottom = height - radius;
        yTop = yBottom - barHeight;
    } else {
        float barHeight = normalized * height;
        yBottom = height;
        yTop = yBottom - barHeight;
    }

    float upperY = min(yTop, yBottom);
    float lowerY = max(yTop, yBottom);

    float centerX = float(index) * cell + radius;
    float signedDistance;
    if (rounded) {
        signedDistance = distanceToSegment(px, vec2(centerX, upperY), vec2(centerX, lowerY)) - radius;
    } else {
        vec2 center = vec2(centerX, (upperY + lowerY) * 0.5);
        vec2 halfSize = vec2(radius, (lowerY - upperY) * 0.5);
        signedDistance = boxDistance(px, center, halfSize);
    }

    float aa = edgeWidth();
    float cov = coverage(signedDistance, aa);
    if (cov <= 0.0) {
        discard;
    }
    return activeColor(px) * cov;
}

vec4 renderBlocks(vec2 px)
{
    float blockWidth = max(p1.x, 1.0);
    float columnSpacing = max(p1.y, 0.0);
    float columnCell = blockWidth + columnSpacing;
    if (p0.w < 1.0) {
        discard;
    }
    int count = int(p0.w);
    int column = int(clamp(floor(px.x / columnCell), 0.0, float(count - 1)));
    float dx = abs(px.x - (float(column) * columnCell + blockWidth * 0.5)) - blockWidth * 0.5;

    float height = max(p0.y, 1.0);
    float blockHeight = max(p3.z, 1.0);
    float blockSpacing = max(p3.w, 0.0);
    float rowStep = blockHeight + blockSpacing;
    int totalRows = int(floor((height + blockSpacing) / rowStep));
    if (totalRows > 1 && mod(float(totalRows), 2.0) < 0.5) {
        totalRows--;
    }
    if (totalRows < 1) {
        discard;
    }

    float value = valueAt(column);
    float activeRows = floor((value / height) * float(totalRows));
    bool centered = p1.z > 0.5;
    bool drawInactive = p3.y > 0.5;
    bool isActive = false;
    float dy;

    if (centered) {
        float halfRows = floor(float(totalRows) * 0.5);
        float centerY = height * 0.5;
        int row = int(clamp(round((px.y - centerY) / rowStep), -halfRows, halfRows));
        dy = abs(px.y - (centerY + float(row) * rowStep)) - blockHeight * 0.5;
        isActive = abs(float(row)) < ceil(activeRows * 0.5);
    } else {
        int row = int(clamp(round((height - blockHeight * 0.5 - px.y) / rowStep),
                            0.0, float(totalRows - 1)));
        float blockBottom = height - float(row) * rowStep;
        dy = abs(px.y - (blockBottom - blockHeight * 0.5)) - blockHeight * 0.5;
        isActive = float(row) < activeRows;
    }

    float aa = edgeWidth();
    float cov = coverage(dx, aa) * coverage(dy, aa);
    if (cov <= 0.0) {
        discard;
    }

    if (isActive) {
        return activeColor(px) * cov;
    }
    if (drawInactive) {
        return inactiveColor(px) * cov;
    }

    discard;
    return vec4(0.0);
}

float waveY(int index)
{
    float height = max(p0.y, 1.0);
    float barWidth = max(p1.x, 1.0);
    bool centered = (p1.z > 0.5) && (p4.y < 0.5);
    float yBottom = centered ? height * 0.5 : height - barWidth * 0.5;
    float normalized = valueAt(index) / height;

    if (centered && p4.z > 0.5 && p4.y < 0.5) {
        int count = int(p0.w);
        normalized *= ((mod(float(index), 2.0) < 0.5) || (index == count - 1)) ? -1.0 : 1.0;
    }

    return yBottom - normalized * (yBottom - barWidth);
}

vec2 wavePoint(int index)
{
    return vec2(float(index) * max(p1.y, 1.0), waveY(index));
}

vec2 waveMid(int index)
{
    return (wavePoint(index) + wavePoint(index + 1)) * 0.5;
}

void curveSegment(int segment, out vec2 start, out vec2 control, out vec2 end)
{
    int count = int(p0.w);
    if (segment == 0) {
        start = wavePoint(0);
        control = start;
        end = (count == 2) ? wavePoint(1) : waveMid(0);
    } else {
        start = waveMid(segment - 1);
        control = wavePoint(segment);
        end = (segment >= count - 2) ? wavePoint(count - 1) : waveMid(segment);
    }
}

float curvePathY(float x, out vec2 tangent)
{
    int count = int(p0.w);
    float spacing = max(p1.y, 1.0);
    int segment = int(clamp(floor(x / spacing + 0.5), 0.0, float(count - 2)));

    vec2 start;
    vec2 control;
    vec2 end;
    curveSegment(segment, start, control, end);

    float a = start.x - 2.0 * control.x + end.x;
    float b = 2.0 * (control.x - start.x);
    float c = start.x - x;
    float t;

    if (abs(a) < 1e-6) {
        t = (abs(b) < 1e-6) ? 0.0 : clamp(-c / b, 0.0, 1.0);
    } else {
        float discriminant = max(b * b - 4.0 * a * c, 0.0);
        float root = sqrt(discriminant);
        float t1 = (-b + root) / (2.0 * a);
        float t2 = (-b - root) / (2.0 * a);
        t = (t1 >= 0.0 && t1 <= 1.0) ? t1 : t2;
        t = clamp(t, 0.0, 1.0);
    }

    float oneMinusT = 1.0 - t;
    float y = oneMinusT * oneMinusT * start.y
        + 2.0 * oneMinusT * t * control.y
        + t * t * end.y;

    tangent = vec2(2.0 * oneMinusT * (control.x - start.x) + 2.0 * t * (end.x - control.x),
                   2.0 * oneMinusT * (control.y - start.y) + 2.0 * t * (end.y - control.y));
    return y;
}

float curveSegmentDistance(vec2 px, int segment)
{
    vec2 start;
    vec2 control;
    vec2 end;
    curveSegment(segment, start, control, end);

    const int subdivisions = 8;
    vec2 previous = start;
    float best = 1e9;
    for (int i = 1; i <= subdivisions; ++i) {
        float t = float(i) / float(subdivisions);
        float oneMinusT = 1.0 - t;
        vec2 point = oneMinusT * oneMinusT * start
            + 2.0 * oneMinusT * t * control
            + t * t * end;
        best = min(best, distanceToSegment(px, previous, point));
        previous = point;
    }
    return best;
}

float curveDistance(vec2 px)
{
    int count = int(p0.w);
    float spacing = max(p1.y, 1.0);
    int segment = int(clamp(floor(px.x / spacing + 0.5), 0.0, float(count - 2)));
    float best = 1e9;

    for (int i = segment - 1; i <= segment + 1; ++i) {
        if (i >= 0 && i <= count - 2) {
            best = min(best, curveSegmentDistance(px, i));
        }
    }
    return best;
}

float triangleDistance(vec2 px)
{
    int count = int(p0.w);
    float spacing = max(p1.y, 1.0);
    int segment = int(clamp(floor(px.x / spacing), 0.0, float(count - 2)));
    float best = 1e9;

    for (int i = segment - 1; i <= segment + 1; ++i) {
        if (i >= 0 && i < count - 1) {
            best = min(best, distanceToSegment(px, wavePoint(i), wavePoint(i + 1)));
        }
    }
    return best;
}

float squareDistance(vec2 px)
{
    int count = int(p0.w);
    float spacing = max(p1.y, 1.0);
    int column = int(clamp(floor(px.x / spacing), 0.0, float(count - 1)));
    float best = abs(px.y - waveY(column));

    if (column >= 1) {
        best = min(best,
                   distanceToSegment(px,
                                     vec2(float(column) * spacing, waveY(column - 1)),
                                     vec2(float(column) * spacing, waveY(column))));
    }
    if (column + 1 <= count - 1) {
        best = min(best,
                   distanceToSegment(px,
                                     vec2(float(column + 1) * spacing, waveY(column)),
                                     vec2(float(column + 1) * spacing, waveY(column + 1))));
    }
    return best;
}

vec4 renderWave(vec2 px)
{
    int count = int(p0.w);
    if (count < 2) {
        discard;
    }

    float height = max(p0.y, 1.0);
    float barWidth = max(p1.x, 1.0);
    float spacing = max(p1.y, 1.0);
    bool centered = (p1.z > 0.5) && (p4.y < 0.5);
    float yBottom = centered ? height * 0.5 : height - barWidth * 0.5;
    float fillYBottom = centered
        ? ((p4.z > 0.5 && p4.y < 0.5) ? height * 0.5 : yBottom)
        : height;

    float pathY;
    float strokeDistance;

    if (p2.w > 1.5) {
        int segment = int(clamp(floor(px.x / spacing), 0.0, float(count - 2)));
        float t = clamp((px.x - float(segment) * spacing) / spacing, 0.0, 1.0);
        pathY = mix(waveY(segment), waveY(segment + 1), t);
        strokeDistance = triangleDistance(px);
    } else if (p2.w > 0.5) {
        int column = int(clamp(floor(px.x / spacing), 0.0, float(count - 1)));
        pathY = waveY(column);
        strokeDistance = squareDistance(px);
    } else {
        vec2 tangent;
        pathY = curvePathY(px.x, tangent);
        strokeDistance = curveDistance(px);
    }

    float aa = edgeWidth();
    float strokeCov = coverage(strokeDistance - barWidth * 0.5, aa);
    float fillCov = 0.0;
    if (p3.x > 0.5) {
        float top = min(pathY, fillYBottom);
        float bottom = max(pathY, fillYBottom);
        fillCov = coverage(max(top - px.y, px.y - bottom), aa);
    }

    if (strokeCov <= 0.0 && fillCov <= 0.0) {
        discard;
    }

    // Canvas draws the stroke first and the fill on top of it.
    vec4 color = activeColor(px) * strokeCov;
    if (fillCov > 0.0) {
        vec4 fill = waveFillColor(px) * fillCov;
        color = fill + color * (1.0 - fill.a);
    }
    return color;
}

void main()
{
    vec2 px = v_uv * p0.xy;

    if (p2.z > 1.5) {
        fragColor = renderBlocks(px);
    } else if (p2.z > 0.5) {
        fragColor = renderWave(px);
    } else {
        fragColor = renderBars(px);
    }
}
